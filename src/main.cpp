#include "shared/VulkanApp.h"

#include "Bistro.h"
#include "Skybox.h"
#include "VKMesh11.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize2.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include "shared/LineCanvas.h"

struct LightParams {
	float theta          = +90.0f;
	float phi            = -26.0f;
	float depthBiasConst = 1.1f;
	float depthBiasSlope = 2.0f;

	bool operator==(const LightParams&) const = default;
} light;

struct LightData {
	mat4 viewProjBias;
	vec4 lightDir;
	uint32_t shadowTexture;
	uint32_t shadowSampler;
};

const struct PushConstant {
	mat4 viewProj;
	uint64_t bufferTransforms;
	uint64_t bufferDrawData;
	uint64_t bufferMaterials;
	uint64_t bufferLight;
	uint32_t texSkyboxIrradiance;
};

int main()
{
    MeshData meshData;
    Scene scene;
    loadBistro(meshData, scene, "../../data/bistro/Exterior/exterior.obj", "../../data/bistro/Interior/interior.obj");

    VulkanApp app({
        .initialCameraPos = vec3(-19.261f, 8.465f, -7.317f),
        .initialCameraTarget = vec3(0, +2.5f, 0),
    });

    app.positioner_.maxSpeed_ = 1.5f;

    std::unique_ptr<lvk::IContext> ctx(app.ctx_.get());

    const lvk::Dimensions sizeFb = ctx->getDimensions(ctx->getCurrentSwapchainTexture());
    const uint32_t kNumSamples = 2;

    lvk::Holder<lvk::TextureHandle> msaaColor = ctx->createTexture({
        .format = ctx->getSwapchainFormat(),
        .dimensions = sizeFb,
        .numSamples = kNumSamples,
        .usage = lvk::TextureUsageBits_Attachment,
        .storage = lvk::StorageType_Memoryless,
        .debugName = "msaaColor",
    });

    lvk::Holder<lvk::TextureHandle> msaaDepth = ctx->createTexture({
        .format = app.getDepthFormat(),
        .dimensions = sizeFb,
        .numSamples = kNumSamples,
        .usage = lvk::TextureUsageBits_Attachment,
        .storage = lvk::StorageType_Memoryless,
        .debugName = "msaaDepth",
    });

	lvk::Holder<lvk::TextureHandle> shadowMap = ctx->createTexture({
		.type       = lvk::TextureType_2D,
		.format     = lvk::Format_Z_UN16,
		.dimensions = { 4096, 4096 },
		.usage      = lvk::TextureUsageBits_Attachment | lvk::TextureUsageBits_Sampled,
		.swizzle    = { .r = lvk::Swizzle_R, .g = lvk::Swizzle_R, .b = lvk::Swizzle_R, .a = lvk::Swizzle_1 },
		.debugName  = "Shadow map",
	});
  
	lvk::Holder<lvk::SamplerHandle> samplerShadow = ctx->createSampler({
		.wrapU               = lvk::SamplerWrap_Clamp,
		.wrapV               = lvk::SamplerWrap_Clamp,
		.depthCompareOp      = lvk::CompareOp_LessEqual,
		.depthCompareEnabled = true,
		.debugName           = "Sampler shadow",
	});

	lvk::Holder<lvk::BufferHandle> bufferLight = ctx->createBuffer({
		.usage     = lvk::BufferUsageBits_Storage,
		.storage   = lvk::StorageType_Device,
		.size      = sizeof(LightData),
		.debugName = "Buffer light",
	});

    lvk::Holder<lvk::ShaderModuleHandle> compCulling = loadShaderModule(ctx, "../../src/shaders/culling/FrustumCulling.comp");
    lvk::Holder<lvk::ComputePipelineHandle> pipelineCulling = ctx->createComputePipeline({
        .smComp = compCulling,
    });

    const Skybox skyBox(
        ctx, 
		"../../data/immenstadter_horn_2k_prefilter.ktx", 
		"../../data/immenstadter_horn_2k_irradiance.ktx", 
        ctx->getSwapchainFormat(),
        app.getDepthFormat(), 
        kNumSamples
    );

    const VKMesh11 mesh(ctx, meshData, scene, lvk::StorageType_Device);

	const VKPipeline11 pipelineMesh(
		ctx, meshData.streams, ctx->getSwapchainFormat(), app.getDepthFormat(), kNumSamples,
		loadShaderModule(ctx, "../../src/shaders/directional_shadow/main.vert"), 
		loadShaderModule(ctx, "../../src/shaders/directional_shadow/main.frag")
	);

	const VKPipeline11 pipelineShadow(
		ctx, meshData.streams, lvk::Format_Invalid, ctx->getFormat(shadowMap), 1,
		loadShaderModule(ctx, "../../src/shaders/directional_shadow/shadow.vert"), 
		loadShaderModule(ctx, "../../src/shaders/directional_shadow/shadow.frag")
	);

    std::vector<BoundingBox> reorderedBoxes;
    reorderedBoxes.resize(scene.globalTransform.size());

    // pretransform bounding boxes to world space
    for (auto &p : scene.meshForNode) {
        reorderedBoxes[p.first] = meshData.boxes[p.second].getTransformed(scene.globalTransform[p.first]);
    }

    lvk::Holder<lvk::BufferHandle> bufferAABBs = ctx->createBuffer({
        .usage = lvk::BufferUsageBits_Storage,
        .storage = lvk::StorageType_Device,
        .size = reorderedBoxes.size() * sizeof(BoundingBox),
        .data = reorderedBoxes.data(),
        .debugName = "Buffer: AABBs",
    });

    struct CullingData
    {
        vec4 frustumPlanes[6];
        vec4 frustumCorners[8];
        uint32_t numMeshesToCull = 0;
        uint32_t numVisibleMeshes = 0; // GPU
    } emptyCullingData;

    int numVisibleMeshes = 0; // CPU

    // round-robin
    const lvk::BufferDesc cullingDataDesc = {
        .usage = lvk::BufferUsageBits_Storage,
        .storage = lvk::StorageType_HostVisible,
        .size = sizeof(CullingData),
        .data = &emptyCullingData,
        .debugName = "Buffer: CullingData 0",
    };

    lvk::Holder<lvk::BufferHandle> bufferCullingData[] = {
        ctx->createBuffer(cullingDataDesc, "Buffer: CullingData 0"),
        ctx->createBuffer(cullingDataDesc, "Buffer: CullingData 1"),
    };

    lvk::SubmitHandle submitHandle[LVK_ARRAY_NUM_ELEMENTS(bufferCullingData)] = {};
    uint32_t currentBufferId = 0;

    struct
    {
        uint64_t commands;
        uint64_t drawData;
        uint64_t AABBs;
        uint64_t meshes;
    } pcCulling = {
        .commands = ctx->gpuAddress(mesh.indirectBuffer_.bufferIndirect_),
        .drawData = ctx->gpuAddress(mesh.bufferDrawData_),
        .AABBs = ctx->gpuAddress(bufferAABBs),
    };

    CullingData cullingData = {
        .numMeshesToCull = static_cast<uint32_t>(scene.meshForNode.size())
    };

	// create the scene AABB in world space
	BoundingBox bigBoxWS = reorderedBoxes.front();
	for (const auto& b : reorderedBoxes) {
	  bigBoxWS.combinePoint(b.min_);
	  bigBoxWS.combinePoint(b.max_);
	}
  
	// update shadow map
	LightParams prevLight = { .depthBiasConst = 0 };
  
	// clang-format off
	const mat4 scaleBias = mat4(0.5, 0.0, 0.0, 0.0,
								0.0, 0.5, 0.0, 0.0,
								0.0, 0.0, 1.0, 0.0,
								0.5, 0.5, 0.0, 1.0);
	// clang-format on

    app.run([&](uint32_t width, uint32_t height, float aspectRatio, float deltaSeconds) {
        const mat4 view = app.camera_.getViewMatrix();
        const mat4 proj = glm::perspective(45.0f, aspectRatio, 0.1f, 200.0f);

		const glm::mat4 rot1 = glm::rotate(mat4(1.f), glm::radians(light.theta), glm::vec3(0, 1, 0));
		const glm::mat4 rot2 = glm::rotate(rot1, glm::radians(light.phi), glm::vec3(1, 0, 0));
		const vec3 lightDir  = glm::normalize(vec3(rot2 * vec4(0.0f, -1.0f, 0.0f, 1.0f)));
		const mat4 lightView = glm::lookAt(glm::vec3(0.0f), lightDir, vec3(0, 0, 1));

		// transform scene AABB to light space
		const BoundingBox boxLS = bigBoxWS.getTransformed(lightView);
		const mat4 lightProj = glm::orthoLH_ZO(boxLS.min_.x, boxLS.max_.x, boxLS.min_.y, boxLS.max_.y, boxLS.max_.z, boxLS.min_.z);

		PushConstant pc {
			.viewProj            = proj * view,
			.bufferTransforms    = ctx->gpuAddress(mesh.bufferTransforms_),
			.bufferDrawData      = ctx->gpuAddress(mesh.bufferDrawData_),
			.bufferMaterials     = ctx->gpuAddress(mesh.bufferMaterials_),
			.bufferLight         = ctx->gpuAddress(bufferLight),
			.texSkyboxIrradiance = skyBox.texSkyboxIrradiance.index()
		};

        lvk::ICommandBuffer& buf = ctx->acquireCommandBuffer();
        {
			// update shadow map
            buf.cmdPushDebugGroupLabel("Shadow map", 0xff0000ff);

			buf.cmdBeginRendering(
                lvk::RenderPass{
                    .depth = {.loadOp = lvk::LoadOp_Clear, .clearDepth = 1.0f}
                },
                lvk::Framebuffer{ .depthStencil = { .texture = shadowMap } }
            );
            
            buf.cmdSetDepthBias(light.depthBiasConst, light.depthBiasSlope);
            buf.cmdSetDepthBiasEnable(true);
            mesh.draw(buf, pipelineShadow, lightView, lightProj);
            buf.cmdSetDepthBiasEnable(false);            
            buf.cmdEndRendering();
            buf.cmdUpdateBuffer(
                bufferLight, 
                LightData{
                    .viewProjBias  = scaleBias * lightProj * lightView,
                    .lightDir      = vec4(lightDir, 0.0f),
                    .shadowTexture = shadowMap.index(),
                    .shadowSampler = samplerShadow.index(),
                }
            );

            buf.cmdPopDebugGroupLabel();

            // 0. Cull scene
            getFrustumPlanes(proj * view, cullingData.frustumPlanes);
            getFrustumCorners(proj * view, cullingData.frustumCorners);
            pcCulling.meshes = ctx->gpuAddress(bufferCullingData[currentBufferId]);

            // cull
            buf.cmdPushDebugGroupLabel("Frustum Culling", 0xff0000ff);

            buf.cmdBindComputePipeline(pipelineCulling);            
            buf.cmdPushConstants(pcCulling);
            buf.cmdUpdateBuffer(bufferCullingData[currentBufferId], cullingData);
            buf.cmdDispatchThreadGroups(
                { 1 + cullingData.numMeshesToCull / 64 }, 
                { .buffers = { lvk::BufferHandle(mesh.indirectBuffer_.bufferIndirect_) } }
            );

            buf.cmdPopDebugGroupLabel();

			const glm::mat4 rot1 = glm::rotate(mat4(1.f), glm::radians(light.theta), glm::vec3(0, 1, 0));
			const glm::mat4 rot2 = glm::rotate(rot1, glm::radians(light.phi), glm::vec3(1, 0, 0));
			const vec3 lightDir  = glm::normalize(vec3(rot2 * vec4(0.0f, -1.0f, 0.0f, 1.0f)));
			const mat4 lightView = glm::lookAt(glm::vec3(0.0f), lightDir, vec3(0, 0, 1));

			// transform scene AABB to light space
			const BoundingBox boxLS = bigBoxWS.getTransformed(lightView);
			const mat4 lightProj = glm::orthoLH_ZO(boxLS.min_.x, boxLS.max_.x, boxLS.min_.y, boxLS.max_.y, boxLS.max_.z, boxLS.min_.z);

            // 1. Render scene
            const lvk::Framebuffer framebufferMSAA = {
                .color        = { { .texture = msaaColor, .resolveTexture = ctx->getCurrentSwapchainTexture() } },
                .depthStencil = { .texture = msaaDepth },
            };
            
            buf.cmdBeginRendering(
                lvk::RenderPass{
                    .color = { { .loadOp = lvk::LoadOp_Clear, .storeOp = lvk::StoreOp_MsaaResolve, .clearColor = { 1.0f, 1.0f, 1.0f, 1.0f } } },
                    .depth = { .loadOp = lvk::LoadOp_Clear, .clearDepth = 1.0f }
                },
                framebufferMSAA, 
				{ 
					.textures = { lvk::TextureHandle(shadowMap) },
					.buffers = { lvk::BufferHandle(mesh.indirectBuffer_.bufferIndirect_) }
				}
            );

            skyBox.draw(buf, view, proj);
            
            buf.cmdPushDebugGroupLabel("Mesh", 0xff0000ff);
			mesh.draw(buf, pipelineMesh, &pc, sizeof(pc), { .compareOp = lvk::CompareOp_Less, .isDepthWriteEnabled = true });
			buf.cmdPopDebugGroupLabel();

            buf.cmdEndRendering();
        }

        submitHandle[currentBufferId] = ctx->submit(buf, ctx->getCurrentSwapchainTexture());
        currentBufferId = (currentBufferId + 1) % LVK_ARRAY_NUM_ELEMENTS(bufferCullingData); 
    });

    ctx.release();

    return 0;
}