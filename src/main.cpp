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
      .initialCameraPos    = vec3(2.13f, 2.44f, -3.5f),
      .initialCameraTarget = vec3(0, 0, 2),
      .showGLTFInspector = true,
  });

  GLTFContext gltf(app);

  loadGLTF(gltf, "../../data/fox/Fox.gltf", "../../data/fox/");

  const mat4 t = glm::translate(mat4(1.0f), vec3(0.0f, 1.1f, 0.0f));
  const mat4 s = glm::scale(mat4(1.0f), vec3(0.01f, 0.01f, 0.01f));

  AnimationState anim = {
    .animId      = 1,
    .currentTime = 0.0f,
    .playOnce    = false,
    .active      = true,
  };
  gltf.skinning       = true;
  gltf.inspector = {
    .activeAnim     = { 1 },
    .showAnimations = true,
  };

  app.run([&](uint32_t width, uint32_t height, float aspectRatio, float deltaSeconds) {
    const mat4 m = t * glm::rotate(mat4(1.0f), glm::radians(90.0f), vec3(0, 1, 0)) * s;
    const mat4 v = app.camera_.getViewMatrix();
    const mat4 p = glm::perspective(45.0f, aspectRatio, 0.01f, 100.0f);
    animateGLTF(gltf, anim, deltaSeconds);
    renderGLTF(gltf, m, v, p);
    if (gltf.inspector.activeAnim[0] != anim.animId) {
      anim = {
        .animId      = gltf.inspector.activeAnim[0],
        .currentTime = 0.0f,
        .playOnce    = false,
        .active      = true,
      };
    }
  });

  return 0;
}