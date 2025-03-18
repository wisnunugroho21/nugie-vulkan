#include <lvk/LVK.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include <unordered_map>

std::unordered_map<uint32_t, std::string> debugGLSLSourceCode;

std::string readShaderFile(const char* fileName) {
    FILE* file = fopen(fileName, "r");

    if (!file) {
        LLOGW("I/O error. Cannot open shader file '%s'\n", fileName);
        return std::string();
    }

    fseek(file, 0L, SEEK_END);
    const size_t bytesinfile = ftell(file);
    fseek(file, 0L, SEEK_SET);

    char* buffer           = (char*)alloca(bytesinfile + 1);
    const size_t bytesread = fread(buffer, 1, bytesinfile, file);
    fclose(file);

    buffer[bytesread] = 0;

    static constexpr unsigned char BOM[] = { 0xEF, 0xBB, 0xBF };

    if (bytesread > 3) {
        if (!memcmp(buffer, BOM, 3))
        memset(buffer, ' ', 3);
    }

    std::string code(buffer);

    while (code.find("#include ") != code.npos) {
        const auto pos = code.find("#include ");
        const auto p1  = code.find('<', pos);
        const auto p2  = code.find('>', pos);

        if (p1 == code.npos || p2 == code.npos || p2 <= p1) {
            LLOGW("Error while loading shader program: %s\n", code.c_str());
            return std::string();
        }

        const std::string name    = code.substr(p1 + 1, p2 - p1 - 1);
        const std::string include = readShaderFile(name.c_str());
        code.replace(pos, p2 - pos + 1, include.c_str());
    }

    return code;
}

bool endsWith(const char* s, const char* part) {
    const size_t sLength    = strlen(s);
    const size_t partLength = strlen(part);
    return sLength < partLength ? false : strcmp(s + sLength - partLength, part) == 0;
}

lvk::ShaderStage lvkShaderStageFromFilename(const char* fileName) {
    if (endsWith(fileName, ".vert"))
        return lvk::Stage_Vert;

    if (endsWith(fileName, ".frag"))
        return lvk::Stage_Frag;

    if (endsWith(fileName, ".geom"))
        return lvk::Stage_Geom;

    if (endsWith(fileName, ".comp"))
        return lvk::Stage_Comp;

    if (endsWith(fileName, ".tesc"))
        return lvk::Stage_Tesc;

    if (endsWith(fileName, ".tese"))
        return lvk::Stage_Tese;

    return lvk::Stage_Vert;
}

lvk::Holder<lvk::ShaderModuleHandle> loadShaderModule(const std::unique_ptr<lvk::IContext>& context, const char* fileName) {
    const std::string code = readShaderFile(fileName);
    const lvk::ShaderStage stage = lvkShaderStageFromFilename(fileName);

    if (code.empty()) {
        return {};
    }

    lvk::Result res;
    lvk::Holder<lvk::ShaderModuleHandle> handle = context->createShaderModule({ 
            code.c_str(), 
            stage, 
            (std::string("Shader Module: ") + fileName).c_str() 
        }, &res);

    if (!res.isOk()) {
        return {};
    }

    debugGLSLSourceCode[handle.index()] = code;
    return handle;
}

int main(int argc, char *argv[]) {
    minilog::initialize(nullptr, { .threadNames = false });
    int width = 960, height = 540;

    GLFWwindow *window = lvk::initWindow("Simple Example", width, height);
    {
        std::unique_ptr<lvk::IContext> context = lvk::createVulkanContextWithSwapchain(window, width, height, {});

        lvk::Holder<lvk::ShaderModuleHandle> vert = loadShaderModule(context, "../../src/main.vert");
        lvk::Holder<lvk::ShaderModuleHandle> frag = loadShaderModule(context, "../../src/main.frag");

        lvk::Holder<lvk::RenderPipelineHandle> pipelineSolid = context->createRenderPipeline({
            .smVert = vert,
            .smFrag = frag,
            .color = {
                {
                    .format = context->getSwapchainFormat()
                }
            },
            .cullMode = lvk::CullMode_Back
        });

        const uint32_t isWireframe = 1;

        lvk::Holder<lvk::RenderPipelineHandle> pipelineWireframe = context->createRenderPipeline({
            .smVert = vert,
            .smFrag = frag,
            .specInfo = {
                .entries = {
                    {
                        .constantId = 0,
                        .size = sizeof(uint32_t)
                    }
                },
                .data = &isWireframe,
                .dataSize = sizeof(isWireframe)
            },
            .color = {
                {
                    .format = context->getSwapchainFormat()
                }
            },
            .cullMode = lvk::CullMode_Back,
            .polygonMode = lvk::PolygonMode_Line
        });

        LVK_ASSERT(pipelineSolid.valid());
        LVK_ASSERT(pipelineWireframe.valid());

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            glfwGetFramebufferSize(window, &width, &height);
            if (!width || !height) {
                continue;
            }

            const float ratio = width / (float) height;

            const glm::mat4 m = glm::rotate(
                glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -3.5f)),
                (float) glfwGetTime(),
                glm::vec3(1.0f, 1.0f, 1.0f)
            );

            const glm::mat4 p = glm::perspective(
                45.0f, ratio, 0.1f, 1000.0f
            );

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

            commandBuffer.cmdPushDebugGroupLabel("Solid Cube", 0xff0000ff);

            {
                commandBuffer.cmdBindRenderPipeline(pipelineSolid);
                commandBuffer.cmdPushConstants(p * m);
                commandBuffer.cmdDraw(36);
            }

            commandBuffer.cmdPopDebugGroupLabel();
            commandBuffer.cmdPushDebugGroupLabel("Wireframe Cube", 0xff0000ff);
            
            {
                commandBuffer.cmdBindRenderPipeline(pipelineWireframe);
                commandBuffer.cmdPushConstants(p * m);
                commandBuffer.cmdDraw(36);
            }

            commandBuffer.cmdPopDebugGroupLabel();            
            commandBuffer.cmdEndRendering();

            context->submit(commandBuffer, context->getCurrentSwapchainTexture());
        }
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
