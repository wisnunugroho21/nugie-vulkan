#include <lvk/LVK.h>
#include <GLFW/glfw3.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize2.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/cimport.h>

#include "shared/Bitmap.h"
#include "shared/Utils.h"
#include "shared/UtilsCubemap.h"

struct VertexData {
    glm::vec3 pos;
    glm::vec3 n;
    glm::vec2 tc;
};

struct PerFrameData {
    glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
	glm::vec4 cameraPos;
	uint32_t tex        = 0;
	uint32_t texCube    = 0;
};

int main(int argc, char *argv[]) {
    minilog::initialize(nullptr, { .threadNames = false });
    int width = 960, height = 540;

    GLFWwindow *window                      = lvk::initWindow("Simple Example", width, height);
    std::unique_ptr<lvk::IContext> context  = lvk::createVulkanContextWithSwapchain(window, width, height, {});

    const aiScene* scene = aiImportFile("../../data/rubber_duck/scene.gltf", aiProcess_Triangulate);

    if (!scene || !scene->HasMeshes()) {
        printf("Unable to load ../../data/rubber_duck/scene.gltf");
        exit(255);
    }

    std::vector<VertexData> vertices;
    std::vector<uint32_t> indices;

    const aiMesh* mesh = scene->mMeshes[0];
    for (uint32_t i = 0; i < mesh->mNumVertices; i++) {
        const aiVector3D v = mesh->mVertices[i];
        const aiVector3D n = mesh->mNormals[i];
        const aiVector3D t = mesh->mTextureCoords[0][i];
        vertices.push_back({ .pos = glm::vec3(v.x, v.y, v.z), .n = glm::vec3(n.x, n.y, n.z), .tc = glm::vec2(t.x, t.y) });
    }

    for (uint32_t i = 0; i < mesh->mNumFaces; i++) {
        for (uint32_t j = 0; j < 3; j++) {
            indices.push_back(mesh->mFaces[i].mIndices[j]);
        }
    }

    aiReleaseImport(scene);

    lvk::Holder<lvk::BufferHandle> vertexBuffer = context->createBuffer({
        .usage      = lvk::BufferUsageBits_Vertex,
        .storage    = lvk::StorageType_Device,
        .size       = sizeof(VertexData) * vertices.size(),
        .data       = vertices.data(),
        .debugName  = "Vertex Buffer"
    });

    lvk::Holder<lvk::BufferHandle> indexBuffer = context->createBuffer({
        .usage      = lvk::BufferUsageBits_Index,
        .storage    = lvk::StorageType_Device,
        .size       = sizeof(uint32_t) * indices.size(),
        .data       = indices.data(),
        .debugName  = "Index Buffer"
    });

    lvk::Holder<lvk::BufferHandle> perFrameBuffer = context->createBuffer({
        .usage      = lvk::BufferUsageBits_Uniform,
        .storage    = lvk::StorageType_Device,
        .size       = sizeof(PerFrameData),
        .debugName  = "Per Frame Buffer"
    });
    
    lvk::Holder<lvk::TextureHandle> depthTexture = context->createTexture({
        .type       = lvk::TextureType_2D,
        .format     = lvk::Format_Z_F32,
        .dimensions = {(uint32_t)width, (uint32_t)height},
        .usage      = lvk::TextureUsageBits_Attachment,
        .debugName  = "Depth Buffer"
    });

    lvk::Holder<lvk::TextureHandle> texture = loadTexture(context, "../../data/rubber_duck/textures/Duck_baseColor.png");

    int w, h, comp;
    const float* img = stbi_loadf("../../data/piazza_bologni_1k.hdr", &w, &h, &comp, 4);
    assert(img);

    Bitmap in(w, h, 4, eBitmapFormat_Float, img);
    Bitmap out = convertEquirectangularMapToVerticalCross(in);
    stbi_image_free((void*)img);
    
    stbi_write_hdr(".cace/screenshot.hdr", out.w_, out.h_, out.comp_, (const float*)out.data_.data());

    Bitmap cubemap = convertVerticalCrossToCubeMapFaces(out);
    
    lvk::Holder<lvk::TextureHandle> cubemapTexture = context->createTexture({
        .type       = lvk::TextureType_Cube,
        .format     = lvk::Format_RGBA_F32,
        .dimensions = { (uint32_t)cubemap.w_, (uint32_t)cubemap.h_ },
        .usage      = lvk::TextureUsageBits_Sampled,
        .data       = cubemap.data_.data(),
        .debugName  = "piazza_bologni_1k.hdr"
    });
    
    lvk::Holder<lvk::ShaderModuleHandle> vert       = loadShaderModule(context, "../../src/shaders/main.vert");
    lvk::Holder<lvk::ShaderModuleHandle> frag       = loadShaderModule(context, "../../src/shaders/main.frag");
    lvk::Holder<lvk::ShaderModuleHandle> vertSkybox = loadShaderModule(context, "../../src/shaders/cubemap/skybox.vert");
    lvk::Holder<lvk::ShaderModuleHandle> fragSkybox = loadShaderModule(context, "../../src/shaders/cubemap/skybox.frag");

    lvk::Holder<lvk::RenderPipelineHandle> pipeline = context->createRenderPipeline({
        .topology = lvk::Topology_TriangleStrip,
        .vertexInput = 
        {
            .attributes = {
                {
                    .location   = 0,
                    .format     = lvk::VertexFormat::Float3,
                    .offset     = offsetof(VertexData, pos)
                },
                {
                    .location   = 1,
                    .format     = lvk::VertexFormat::Float3,
                    .offset     = offsetof(VertexData, n)
                },
                {
                    .location   = 2,
                    .format     = lvk::VertexFormat::Float2,
                    .offset     = offsetof(VertexData, tc)
                }
            },
            .inputBindings = {
                {
                    .stride = sizeof(VertexData)
                }
            }
        },        
        .smVert = vert,
        .smFrag = frag,
        .color = {
            {
                .format = context->getSwapchainFormat()
            }
        },
        .depthFormat = context->getFormat(depthTexture),
        .cullMode = lvk::CullMode_Back
    });

    LVK_ASSERT(pipeline.valid());

    lvk::Holder<lvk::RenderPipelineHandle> pipelineSkybox = context->createRenderPipeline({        
        .smVert = vertSkybox,
        .smFrag = fragSkybox,
        .color = {
            {
                .format = context->getSwapchainFormat()
            }
        },
        .depthFormat = context->getFormat(depthTexture)
    });

    LVK_ASSERT(pipelineSkybox.valid());

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        glfwGetFramebufferSize(window, &width, &height);
        if (!width || !height) {
            continue;
        }

        const float ratio = width / (float) height;

        const glm::vec3 cameraPos(0.0f, 1.0f, -1.5f);

        const glm::mat4 p  = glm::perspective(glm::radians(60.0f), ratio, 0.1f, 1000.0f);
        const glm::mat4 m1 = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1, 0, 0));
        const glm::mat4 m2 = glm::rotate(glm::mat4(1.0f), (float)glfwGetTime(), glm::vec3(0.0f, 1.0f, 0.0f));
        const glm::mat4 v  = glm::lookAt(cameraPos, glm::vec3(0.0f, 0.5f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        const PerFrameData pc = {
            .model     = m2 * m1,
            .view      = v,
            .proj      = p,
            .cameraPos = glm::vec4(cameraPos, 1.0f),
            .tex       = texture.index(),
            .texCube   = cubemapTexture.index(),
        };

        context->upload(perFrameBuffer, &pc, sizeof(pc));

        lvk::ICommandBuffer& commandBuffer = context->acquireCommandBuffer();

        commandBuffer.cmdBeginRendering(
            {
                .color = {
                    {
                        .loadOp = lvk::LoadOp_Clear,
                        .clearColor = { 1.0f, 1.0f, 1.0f, 1.0f }
                    }
                },
                .depth = {
                    .loadOp = lvk::LoadOp_Clear,
                    .clearDepth = 1.0f
                }
            },
            {
                .color = {
                    {
                        .texture = context->getCurrentSwapchainTexture()
                    }
                },
                .depthStencil = {
                    .texture = depthTexture
                }
            }
        );

        commandBuffer.cmdPushDebugGroupLabel("SkyBox", 0xff0000ff);

        {
            commandBuffer.cmdBindRenderPipeline(pipelineSkybox);
            commandBuffer.cmdPushConstants(context->gpuAddress(perFrameBuffer));
            commandBuffer.cmdDraw(36);
        }

        commandBuffer.cmdPopDebugGroupLabel();        
        commandBuffer.cmdPushDebugGroupLabel("Mesh", 0xff0000ff);

        {
            commandBuffer.cmdBindVertexBuffer(0, vertexBuffer);
            commandBuffer.cmdBindIndexBuffer(indexBuffer, lvk::IndexFormat_UI32);
            commandBuffer.cmdBindRenderPipeline(pipeline);
            commandBuffer.cmdBindDepthState({
                .compareOp = lvk::CompareOp_Less,
                .isDepthWriteEnabled = true
            });
            commandBuffer.cmdDrawIndexed((uint32_t)indices.size());
        }

        commandBuffer.cmdPopDebugGroupLabel();
        commandBuffer.cmdEndRendering();

        context->submit(commandBuffer, context->getCurrentSwapchainTexture());
    }

    vertexBuffer.reset();
    indexBuffer.reset();
    perFrameBuffer.reset();

    vert.reset();
    frag.reset();
    vertSkybox.reset();
    fragSkybox.reset();

    texture.reset();
    depthTexture.reset();
    cubemapTexture.reset();

    pipeline.reset();
    pipelineSkybox.reset();
    context.reset();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
