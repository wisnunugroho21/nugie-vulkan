//
#version 460

layout(push_constant) uniform PerFrameData {
	mat4 MVP;
};

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec4 color;
layout (location = 3) in vec2 uv0;
layout (location = 4) in vec2 uv1;

layout (location = 0) out vec4 fragColor;

void main() {
	gl_Position = MVP * vec4(position, 1.0);
	fragColor = color;
}