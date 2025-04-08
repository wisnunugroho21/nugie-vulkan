//

#include <../../src/shaders/directional_shadow/common.sp>
#include <../../src/shaders/util/AlphaTest.sp>
#include <../../src/shaders/util/UtilsPBR.sp>

layout (location=0) in vec2 uv;
layout (location=1) in flat uint materialId;

void main() {
  MetallicRoughnessDataGPU mat = pc.materials.material[materialId];

  if (mat.emissiveFactorAlphaCutoff.w > 0.5) discard;
}