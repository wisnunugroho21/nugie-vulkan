#version 460 core

layout(push_constant) uniform PerFrameData {
    mat4 mvp;
};

layout(constant_id = 0) const bool isWireframe = false;

layout(location = 0) in vec3 pos;
layout(location = 0) out vec3 color;

void main() {
    gl_Position = mvp * vec4(pos, 1.0f);
    color = isWireframe ? vec3(0.0f) : pos.xyz;
}