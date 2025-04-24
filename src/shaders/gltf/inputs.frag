#include <../../src/shaders/gltf/common.sp>

const int kMaxAttributes = 2;

struct InputAttributes {
  vec2 uv[kMaxAttributes];
};

struct Light {
  vec3 direction;
  float range;

  vec3 color;
  float intensity;

  vec3 position;
  float innerConeCos;

  float outerConeCos;
  int type;
  int padding[2];
};

const int LightType_Directional = 0;
const int LightType_Point = 1;
const int LightType_Spot = 2;

struct EnvironmentMapDataGPU {
  uint envMapTexture;
  uint envMapTextureSampler;
  uint envMapTextureIrradiance;
  uint envMapTextureIrradianceSampler;
  uint texBRDFLUT;
  uint texBRDFLUTSampler;
  uint envMapTextureCharlie;
  uint envMapTextureCharlieSampler;
};

layout(std430, buffer_reference) readonly buffer Environments {
  EnvironmentMapDataGPU environment[];
};

layout(std430, buffer_reference) readonly buffer Lights {
  Light lights[];
};

EnvironmentMapDataGPU getEnvironmentMap(uint idx) {
  return perFrame.environments.environment[idx]; 
}

uint getLightsCount() {
  return perFrame.lightsCount;
}

Light getLight(uint i) {
  return perFrame.lights.lights[i];
}

mat4 getModel() {
  uint mtxId = perFrame.transforms.transforms[oBaseInstance].mtxId;
  return perFrame.drawable.model * perFrame.matrices.matrix[mtxId];
}