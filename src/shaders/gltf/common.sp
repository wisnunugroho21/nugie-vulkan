//

// gl_BaseInstance - transformId

layout(std430, buffer_reference) buffer PerDrawData {
  mat4 model;
  mat4 view;
  mat4 proj;
  vec4 cameraPos;
};

struct NodeTransformRef {
  uint mtxId;
  uint nodeRef; // for CPU only
  uint meshRef; // for CPU only
};

layout(std430, buffer_reference) readonly buffer NodeTransformRefs {
  NodeTransformRef refs[];
};

layout(std430, buffer_reference) readonly buffer Matrices {
  mat4 matrix[];
};

layout(push_constant) uniform PerFrameData {
  PerDrawData drawable;
  NodeTransformRefs nodeTransform;
  Matrices matrices;
} perFrame;

mat4 getViewProjection() {
  return perFrame.drawable.proj * perFrame.drawable.view;
}