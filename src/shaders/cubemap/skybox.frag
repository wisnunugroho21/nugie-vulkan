//

#include <../../src/shaders/cubemap/common.sp>

layout (location=0) in vec3 dir;

layout (location=0) out vec4 out_FragColor;

void main() {
  vec3 sky = vec3(-dir.x, dir.y, -dir.z); // rotate skybox

  out_FragColor = textureBindlessCube(pc.texSkybox, 0, sky);
};