//

layout (location=0) in vec4 uv0uv1;
layout (location=1) in vec3 normal;
layout (location=2) in vec3 worldPos;
layout (location=3) in vec4 color;
layout (location=4) in flat int oBaseInstance;

layout (location=0) out vec4 out_FragColor;

#include <../../src/shaders/gltf/inputs.frag>

void main()
{
  out_FragColor = vec4(0.5f, 0.5f, 0.5f, 1.0);
}
