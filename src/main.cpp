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

#include <shared/SkeletalMesh.h>

int main()
{
	VulkanApp app({
		.initialCameraPos = vec3(0.0f, 3.0f, 3.0f),
		.initialCameraTarget = vec3(0.0f, 3.0f, 0.0f)
	});

	SkeletalMesh gltf(app.ctx_.get(), app.getDepthFormat());

	load(gltf, "../../data/bikini_girl/Bikini_Girl_Source.gltf", "../../data/bikini_girl/");

	gltf.skinning = true;
	AnimationState anim = {
		.animId      = 0,
		.currentTime = 0.0f,
		.playOnce    = false,
		.active      = true,
	};

	const mat4 t = glm::translate(mat4(1.0f), vec3(0.0f, -1.0f, 0.0f)) * glm::scale(mat4(1.0f), vec3(1.0f));

	app.run([&](uint32_t width, uint32_t height, float aspectRatio, float deltaSeconds) {
		const mat4 m = t * glm::rotate(mat4(1.0f), glm::radians(0.0f), vec3(0, 1, 0));
		const mat4 v = app.camera_.getViewMatrix();
		const mat4 p = glm::perspective(45.0f, aspectRatio, 0.01f, 200.0f);

		animate(gltf, anim, deltaSeconds);
		render(gltf, app.getDepthTexture(), m, v, p);
	});

	return 0;
}