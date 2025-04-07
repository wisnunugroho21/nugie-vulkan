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

const float M_PI = 3.141592653589793;

vec4 SRGBtoLINEAR(vec4 srgbIn) {
  vec3 linOut = pow(srgbIn.xyz,vec3(2.2));

  return vec4(linOut, srgbIn.a);
}

// http://www.thetenthplanet.de/archives/1180
// modified to fix handedness of the resulting cotangent frame
mat3 cotangentFrame( vec3 N, vec3 p, vec2 uv ) {
  // get edge vectors of the pixel triangle
  vec3 dp1 = dFdx( p );
  vec3 dp2 = dFdy( p );
  vec2 duv1 = dFdx( uv );
  vec2 duv2 = dFdy( uv );

  // solve the linear system
  vec3 dp2perp = cross( dp2, N );
  vec3 dp1perp = cross( N, dp1 );
  vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
  vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;

  // construct a scale-invariant frame
  float invmax = inversesqrt( max( dot(T,T), dot(B,B) ) );

  // calculate handedness of the resulting cotangent frame
  float w = (dot(cross(N, T), B) < 0.0) ? -1.0 : 1.0;

  // adjust tangent if needed
  T = T * w;

  return mat3( T * invmax, B * invmax, N );
}

vec3 perturbNormal(vec3 n, vec3 v, vec3 normalSample, vec2 uv) {
  vec3 map = normalize( 2.0 * normalSample - vec3(1.0) );
  mat3 TBN = cotangentFrame(n, v, uv);
  return normalize(TBN * map);
}

layout (early_fragment_tests) in;

layout (set = 0, binding = 2, r32ui) uniform uimage2D kTextures2DInOut[];

layout (location=0) in vec2 uv;
layout (location=1) in vec3 normal;
layout (location=2) in vec3 worldPos;
layout (location=3) in flat uint materialId;

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
  return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
  MetallicRoughnessDataGPU mat = pc.materials.material[materialId];

  vec4 emissiveColor = vec4(mat.emissiveFactorAlphaCutoff.rgb, 0) * textureBindless2D(mat.emissiveTexture, 0, uv);
  vec4 baseColor = mat.baseColorFactor * (mat.baseColorTexture > 0 ? textureBindless2D(mat.baseColorTexture, 0, uv) : vec4(1.0));

  // world-space normal
  vec3 n = normalize(normal);

  // normal mapping: skip missing normal maps
  vec3 normalSample = textureBindless2D(mat.normalTexture, 0, uv).xyz;
  if (length(normalSample) > 0.5)
    n = perturbNormal(n, worldPos, normalSample, uv);

  // two hardcoded directional lights
  float NdotL1 = clamp(dot(n, normalize(vec3(-1, 1,+0.5))), 0.1, 1.0);
  float NdotL2 = clamp(dot(n, normalize(vec3(+1, 1,-0.5))), 0.1, 1.0);
  float NdotL = 0.2 * (NdotL1+NdotL2);

  // IBL diffuse - not trying to be PBR-correct here, just make it simple & shiny
  const vec4 f0 = vec4(0.04);
  vec3 sky = vec3(-n.x, n.y, -n.z); // rotate skybox
  vec4 diffuse = (textureBindlessCube(pc.texSkyboxIrradiance, 0, sky) + vec4(NdotL)) * baseColor * (vec4(1.0) - f0);
  // some ad hoc environment reflections for transparent objects
  vec3 v = normalize(pc.cameraPos.xyz - worldPos);
  vec3 reflection = reflect(v, n);
  reflection = vec3(reflection.x, -reflection.y, reflection.z); // rotate reflection
  vec3 colorRefl = textureBindlessCube(pc.texSkybox, 0, reflection).rgb;
  vec3 kS = fresnelSchlickRoughness(clamp(dot(n, v), 0.0, 1.0), vec3(f0), 0.1);
  vec3 color = emissiveColor.rgb + diffuse.rgb + colorRefl * kS;

  // Order-Independent Transparency: https://fr.slideshare.net/hgruen/oit-and-indirect-illumination-using-dx11-linked-lists
  float alpha = clamp(baseColor.a * mat.clearcoatTransmissionThickness.z, 0.0, 1.0);
  bool isTransparent = (alpha > 0.01) && (alpha < 0.99);
  uint mask = 1 << gl_SampleID;
  if (isTransparent && !gl_HelperInvocation && ((gl_SampleMaskIn[0] & mask) == mask)) {
    uint index = atomicAdd(pc.oit.atomicCounter.numFragments, 1);
    if (index < pc.oit.maxOITFragments) {
      uint prevIndex = imageAtomicExchange(kTextures2DInOut[pc.oit.texHeadsOIT], ivec2(gl_FragCoord.xy), index);
      TransparentFragment frag;
      frag.color = f16vec4(color, alpha);
      frag.depth = gl_FragCoord.z;
      frag.next  = prevIndex;
      pc.oit.oitLists.frags[index] = frag;
    }
  }
}