#include "shared/VulkanApp.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize2.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

int main() {
	VulkanApp app({
		.initialCameraPos = vec3(0, 0, -5.0f),
		.initialCameraTarget = vec3(0, 0, 0)
	});

	lvk::Holder<lvk::ShaderModuleHandle> vert = loadShaderModule(app.ctx_.get(), "../../src/shaders/main.vert");
	lvk::Holder<lvk::ShaderModuleHandle> frag = loadShaderModule(app.ctx_.get(), "../../src/shaders/main.frag");

	lvk::Holder<lvk::RenderPipelineHandle> rpTriangle = app.ctx_->createRenderPipeline({
		.smVert = vert,
		.smFrag = frag,
		.color = {
			{
				.format = app.ctx_->getSwapchainFormat()
			}
		}
	});

	app.run([&](uint32_t width, uint32_t height, float deltaSeconds) {
		lvk::ICommandBuffer& commandBuf = app.ctx_->acquireCommandBuffer();

		commandBuf.cmdBeginRendering({
			.color = {
				{
					.loadOp = lvk::LoadOp_Clear,
					.clearColor = { 1.0f, 1.0f, 1.0f, 1.0f }
				}
			}
		}, {
			.color = {
				{
					.texture = app.ctx_->getCurrentSwapchainTexture()
				}
			}
		});

		commandBuf.cmdBindRenderPipeline(rpTriangle);
		commandBuf.cmdPushDebugGroupLabel("Render Triangle", 0xff0000ff);
		commandBuf.cmdDraw(3);
		commandBuf.cmdPopDebugGroupLabel();

		commandBuf.cmdEndRendering();

		app.ctx_->submit(commandBuf, app.ctx_->getCurrentSwapchainTexture());
	});
	
	return 0;
}