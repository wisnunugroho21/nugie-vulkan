//

struct MetallicRoughnessDataGPU {
  vec4 baseColorFactor;
  vec4 metallicRoughnessNormalOcclusion; // Packed metallicFactor, roughnessFactor, normalScale, occlusionStrength
  vec4 specularGlossiness; // Packed specularFactor.xyz, glossiness 
  vec4 sheenFactors;
  vec4 clearcoatTransmissionThickness;
  vec4 specularFactors;
  vec4 attenuation;
  vec4 emissiveFactorAlphaCutoff; // vec3 emissiveFactor + float AlphaCutoff
  uint occlusionTexture;
  uint occlusionTextureSampler;
  uint occlusionTextureUV;
  uint emissiveTexture;
  uint emissiveTextureSampler;
  uint emissiveTextureUV;
  uint baseColorTexture;
  uint baseColorTextureSampler;
  uint baseColorTextureUV;
  uint metallicRoughnessTexture;
  uint metallicRoughnessTextureSampler;
  uint metallicRoughnessTextureUV;
  uint normalTexture;
  uint normalTextureSampler;
  uint normalTextureUV;
  uint sheenColorTexture;
  uint sheenColorTextureSampler;
  uint sheenColorTextureUV;
  uint sheenRoughnessTexture;
  uint sheenRoughnessTextureSampler;
  uint sheenRoughnessTextureUV;
  uint clearCoatTexture;
  uint clearCoatTextureSampler;
  uint clearCoatTextureUV;
  uint clearCoatRoughnessTexture;
  uint clearCoatRoughnessTextureSampler;
  uint clearCoatRoughnessTextureUV;
  uint clearCoatNormalTexture;
  uint clearCoatNormalTextureSampler;
  uint clearCoatNormalTextureUV;
  uint specularTexture;
  uint specularTextureSampler;
  uint specularTextureUV;
  uint specularColorTexture;
  uint specularColorTextureSampler;
  uint specularColorTextureUV;
  uint transmissionTexture;
  uint transmissionTextureSampler;
  uint transmissionTextureUV;
  uint thicknessTexture;
  uint thicknessTextureSampler;
  uint thicknessTextureUV;
  uint iridescenceTexture;
  uint iridescenceTextureSampler;
  uint iridescenceTextureUV;
  uint iridescenceThicknessTexture;
  uint iridescenceThicknessTextureSampler;
  uint iridescenceThicknessTextureUV;
  uint anisotropyTexture;
  uint anisotropyTextureSampler;
  uint anisotropyTextureUV;
  uint alphaMode;
  uint materialType;
  float ior;
  uint padding[2];
};

struct TransparentFragment {
  f16vec4 color;
  float depth;
  uint next;
};

layout(std430, buffer_reference) buffer TransparencyListsBuffer {
  TransparentFragment frags[];
};

struct DrawData {
  uint transformId;
  uint materialId;
};

layout(std430, buffer_reference) readonly buffer TransformBuffer {
  mat4 model[];
};

layout(std430, buffer_reference) readonly buffer DrawDataBuffer {
  DrawData dd[];
};

layout(std430, buffer_reference) readonly buffer MaterialBuffer {
  MetallicRoughnessDataGPU material[];
};

layout(std430, buffer_reference) buffer AtomicCounter {
  uint numFragments;
};

layout(std430, buffer_reference) buffer OIT {
  AtomicCounter atomicCounter;
  TransparencyListsBuffer oitLists;
  uint texHeadsOIT;
  uint maxOITFragments;
};

layout(push_constant) uniform PerFrameData {
  mat4 viewProj;
  vec4 cameraPos;
  TransformBuffer transforms;
  DrawDataBuffer drawData;
  MaterialBuffer materials;
  OIT oit;
  uint texSkybox;
  uint texSkyboxIrradiance;
} pc;

layout (location=0) in vec3 in_pos;
layout (location=1) in vec2 in_tc;
layout (location=2) in vec3 in_normal;

layout (location=0) out vec2 uv;
layout (location=1) out vec3 normal;
layout (location=2) out vec3 worldPos;
layout (location=3) out flat uint materialId;

void main() {
  mat4 model = pc.transforms.model[pc.drawData.dd[gl_BaseInstance].transformId];
  gl_Position = pc.viewProj * model * vec4(in_pos, 1.0);
  uv = vec2(in_tc.x, 1.0-in_tc.y);
  normal = transpose( inverse(mat3(model)) ) * in_normal;
  vec4 posClip = model * vec4(in_pos, 1.0);
  worldPos = posClip.xyz/posClip.w;
  materialId = pc.drawData.dd[gl_BaseInstance].materialId;
}