//

#include <../../src/shaders/scenegraph/common.sp>
#include <../../src/shaders/util/AlphaTest.sp>
#include <../../src/shaders/util/UtilsPBR.sp>

layout (location=0) in vec2 uv;
layout (location=1) in flat uint materialId;

layout (location=0) out vec4 out_FragColor;

void main() {
  MetallicRoughnessDataGPU mat = pc.materials.material[materialId];
  vec4 baseColor = mat.baseColorFactor * (mat.baseColorTexture > 0 ? textureBindless2D(mat.baseColorTexture, 0, uv) : vec4(1.0));

  out_FragColor = baseColor;
}