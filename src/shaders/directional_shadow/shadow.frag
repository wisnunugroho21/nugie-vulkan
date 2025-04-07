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

layout(std430, buffer_reference) readonly buffer LightBuffer {
  mat4 viewProjBias;
  vec4 lightDir;
  uint shadowTexture;
  uint shadowSampler;
};

layout(push_constant) uniform PerFrameData {
  mat4 viewProj;
  TransformBuffer transforms;
  DrawDataBuffer drawData;
  MaterialBuffer materials;
  LightBuffer light; // one directional light
  uint texSkyboxIrradiance;
} pc;

void runAlphaTest(float alpha, float alphaThreshold)
{
  if (alphaThreshold > 0.0) {
    // http://alex-charlton.com/posts/Dithering_on_the_GPU/
    // https://forums.khronos.org/showthread.php/5091-screen-door-transparency
    mat4 thresholdMatrix = mat4(
      1.0  / 17.0,  9.0 / 17.0,  3.0 / 17.0, 11.0 / 17.0,
      13.0 / 17.0,  5.0 / 17.0, 15.0 / 17.0,  7.0 / 17.0,
      4.0  / 17.0, 12.0 / 17.0,  2.0 / 17.0, 10.0 / 17.0,
      16.0 / 17.0,  8.0 / 17.0, 14.0 / 17.0,  6.0 / 17.0
    );

    alpha = clamp(alpha - 0.5 * thresholdMatrix[int(mod(gl_FragCoord.x, 4.0))][int(mod(gl_FragCoord.y, 4.0))], 0.0, 1.0);

    if (alpha < alphaThreshold)
      discard;
  }
}

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

layout (location=0) in vec2 uv;
layout (location=1) in flat uint materialId;

void main() {
  MetallicRoughnessDataGPU mat = pc.materials.material[materialId];

  if (mat.emissiveFactorAlphaCutoff.w > 0.5) discard;
}