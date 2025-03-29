//
#version 460

#include "../../src/shaders/common.sp"

layout (location = 0) in vec2 uv;
layout (location = 0) out vec4 out_FragColor;

void main() {
  out_FragColor = textureBindless2D(pc.textureId, 0, uv) * vec4(vec3(1.0), pc.alphaScale);
}