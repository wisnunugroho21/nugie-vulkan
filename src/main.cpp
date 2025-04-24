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

#include <shared/UtilsGLTF.h>

int main()
{
	VulkanApp app({
		.initialCameraPos = vec3(0.0f, 3.0f, 3.0f),
		.initialCameraTarget = vec3(0.0f, 3.0f, 0.0f),
		.showGLTFInspector = true
	});

	GLTFContext gltf(app);

	loadGLTF(gltf, "../../data/bikini_girl/Bikini_Girl_Source.gltf", "../../data/bikini_girl/");

	gltf.enableMorphing = false;

	const mat4 t = glm::translate(mat4(1.0f), vec3(0.0f, -1.0f, 0.0f)) * glm::scale(mat4(1.0f), vec3(1.0f));

	app.run([&](uint32_t width, uint32_t height, float aspectRatio, float deltaSeconds) {
		const mat4 m = t * glm::rotate(mat4(1.0f), glm::radians(0.0f), vec3(0, 1, 0));
		const mat4 v = app.camera_.getViewMatrix();
		const mat4 p = glm::perspective(45.0f, aspectRatio, 0.01f, 200.0f);

		renderGLTF(gltf, m, v, p);
	});

	return 0;
}