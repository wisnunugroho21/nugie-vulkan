#version 460 core

layout(push_constant) uniform PerFrameData {
    uniform mat4 MVP;
    uint textureid;
};

layout(location = 0) out vec2 uv;

const vec2 pos[4] = vec2[4](
    vec2( 1.0, -1.0),
	vec2( 1.0,  1.0),
	vec2(-1.0, -1.0),
	vec2(-1.0,  1.0)
);

void main() {
    gl_Position = MVP * vec4(0.5f * pos[gl_VertexIndex], 0.0f, 1.0f);
    uv = 0.5f * (pos[gl_VertexIndex] + vec2(0.5f));
}