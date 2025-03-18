#version 460

layout(location = 0) out vec3 color;

const vec2 pos[3] = vec2[3](
    vec2(-0.5f, -0.5f),
    vec2( 0.5f, -0.5f),
    vec2( 0.0f,  0.5f)
);

const vec3 col[3] = vec3[3](
    vec3(1.0f, 0.0f, 0.0f),
    vec3(0.0f, 1.0f, 0.0f),
    vec3(0.0f, 0.0f, 1.0f)
);

void main() {
    gl_Position = vec4(pos[gl_VertexIndex], 0.0f, 1.0f);
    color = col[gl_VertexIndex];
}