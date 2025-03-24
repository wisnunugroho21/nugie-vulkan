#include <lvk/LVK.h>
#include <GLFW/glfw3.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize2.h>

#include <unordered_map>
#include "shared/Utils.h"

int main(int argc, char *argv[]) {
    minilog::initialize(nullptr, { .threadNames = false });
    int width = 960, height = 540;

    GLFWwindow *window = lvk::initWindow("Simple Example", width, height);
    std::unique_ptr<lvk::IContext> context = lvk::createVulkanContextWithSwapchain(window, width, height, {});

    lvk::Holder<lvk::ShaderModuleHandle> vert = loadShaderModule(context, "../../src/main.vert");
    lvk::Holder<lvk::ShaderModuleHandle> frag = loadShaderModule(context, "../../src/main.frag");

    lvk::Holder<lvk::RenderPipelineHandle> pipeline = context->createRenderPipeline({
        .topology = lvk::Topology_TriangleStrip,
        .smVert = vert,
        .smFrag = frag,
        .color = {
            {
                .format = context->getSwapchainFormat()
            }
        },
        .cullMode = lvk::CullMode_Back
    });

    LVK_ASSERT(pipeline.valid());

    int w, h, comp;
    const uint8_t* img = stbi_load("../../data/wood.jpg", &w, &h, &comp, 4);

    assert(img);

    lvk::Holder<lvk::TextureHandle> texture = context->createTexture({
        .type       = lvk::TextureType_2D,
        .format     = lvk::Format_RGBA_UN8,
        .dimensions = { (uint32_t)w, (uint32_t)h },
        .usage      = lvk::TextureUsageBits_Sampled,
        .data       = img,
        .debugName  = "03_STB.jpg"
    });

    stbi_image_free((void*)img);    

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        glfwGetFramebufferSize(window, &width, &height);
        if (!width || !height) {
            continue;
        }

        const float ratio = width / (float) height;

        const glm::mat4 m = glm::rotate(
            glm::mat4(1.0f), 
            (float)glfwGetTime(), 
            glm::vec3(0.0f, 0.0f, 1.0f)
        );

        const glm::mat4 p = glm::ortho(-ratio, ratio, -1.f, 1.f, 1.f, -1.f);

        const struct PerFrameData {
            glm::mat4 mvp;
            uint32_t textureId;
        } pc = {
            .mvp        = p * m,
            .textureId  = texture.index()
        };

        lvk::ICommandBuffer& commandBuffer = context->acquireCommandBuffer();

        commandBuffer.cmdBeginRendering(
            {
                .color = {
                    {
                        .loadOp = lvk::LoadOp_Clear,
                        .clearColor = { 1.0f, 1.0f, 1.0f, 1.0f }
                    }
                }
            },
            {
                .color = {
                    {
                        .texture = context->getCurrentSwapchainTexture()
                    }
                }
            }
        );

        commandBuffer.cmdPushDebugGroupLabel("Quad", 0xff0000ff);

        {
            commandBuffer.cmdBindRenderPipeline(pipeline);
            commandBuffer.cmdPushConstants(pc);
            commandBuffer.cmdDraw(4);
        }

        commandBuffer.cmdPopDebugGroupLabel();            
        commandBuffer.cmdEndRendering();

        context->submit(commandBuffer, context->getCurrentSwapchainTexture());
    }

    vert.reset();
    frag.reset();
    texture.reset();
    pipeline.reset();
    context.reset();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
