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
		.initialCameraPos = vec3(0, 0, -6.359f),
		.initialCameraTarget = vec3(0, 0, 0)
	});

	app.run([&](uint32_t width, uint32_t height, float aspectRatio, float deltaSeconds) {

	});
	
	return 0;
}