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

mat4 cullingView = mat4(1.0f);
bool freezeCullingView = false;
bool drawMeshes = true;
bool drawBoxes = true;
bool drawWireframe = false;

int main()
{
    MeshData meshData;
    Scene scene;
    loadBistro(meshData, scene);

    VulkanApp app({
        .initialCameraPos = vec3(-19.261f, 8.465f, -7.317f),
        .initialCameraTarget = vec3(0, +2.5f, 0),
    });

    app.positioner_.maxSpeed_ = 1.5f;

    std::unique_ptr<lvk::IContext> ctx(app.ctx_.get());

    const lvk::Dimensions sizeFb = ctx->getDimensions(ctx->getCurrentSwapchainTexture());
    const uint32_t kNumSamples = 4;

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

    lvk::Holder<lvk::ShaderModuleHandle> compCulling = loadShaderModule(ctx, "../../src/shaders/culling/FrustumCulling.comp");
    lvk::Holder<lvk::ComputePipelineHandle> pipelineCulling = ctx->createComputePipeline({
        .smComp = compCulling,
    });

    const Skybox skyBox(
        ctx, "../../data/immenstadter_horn_2k_prefilter.ktx", "../../data/immenstadter_horn_2k_irradiance.ktx", 
        ctx->getSwapchainFormat(),
        app.getDepthFormat(), 
        kNumSamples
    );

    const VKMesh11 mesh(ctx, meshData, scene, lvk::StorageType_Device);
    const VKPipeline11 pipeline(ctx, meshData.streams, ctx->getSwapchainFormat(), app.getDepthFormat(), kNumSamples);

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

    app.run([&](uint32_t width, uint32_t height, float aspectRatio, float deltaSeconds) {
        const mat4 view = app.camera_.getViewMatrix();
        const mat4 proj = glm::perspective(45.0f, aspectRatio, 0.1f, 200.0f);

        lvk::ICommandBuffer& buf = ctx->acquireCommandBuffer();
        {
            // 0. Cull scene
            if (!freezeCullingView)
                cullingView = app.camera_.getViewMatrix();

            CullingData cullingData = {
                .numMeshesToCull = static_cast<uint32_t>(scene.meshForNode.size()),
            };

            getFrustumPlanes(proj * cullingView, cullingData.frustumPlanes);
            getFrustumCorners(proj * cullingView, cullingData.frustumCorners);
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
                { .buffers = { lvk::BufferHandle(mesh.indirectBuffer_.bufferIndirect_) } }
            );

            skyBox.draw(buf, view, proj);
            
            if (drawMeshes) {
                buf.cmdPushDebugGroupLabel("Mesh", 0xff0000ff);
                mesh.draw(buf, pipeline, view, proj, skyBox.texSkyboxIrradiance, drawWireframe);
                buf.cmdPopDebugGroupLabel();
            }

            buf.cmdEndRendering();

            const lvk::Framebuffer framebufferMain = {
                .color = { { .texture = ctx->getCurrentSwapchainTexture() } },
            };

            buf.cmdBeginRendering(
                lvk::RenderPass{
                    .color = { { .loadOp = lvk::LoadOp_Load, .storeOp = lvk::StoreOp_Store } },
                },
                framebufferMain
            );            

            buf.cmdEndRendering();
        }

        submitHandle[currentBufferId] = ctx->submit(buf, ctx->getCurrentSwapchainTexture());
        currentBufferId = (currentBufferId + 1) % LVK_ARRAY_NUM_ELEMENTS(bufferCullingData); 
    });

    ctx.release();

    return 0;
}