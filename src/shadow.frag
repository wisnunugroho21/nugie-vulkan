//

#include <../../src/common_directional_shadow.sp>
#include <../../src/AlphaTest.sp>
#include <../../src/UtilsPBR.sp>

layout (location=0) in vec2 uv;
layout (location=1) in flat uint materialId;

void main() {
  MetallicRoughnessDataGPU mat = pc.materials.material[materialId];

  if (mat.emissiveFactorAlphaCutoff.w > 0.5) discard;
}
