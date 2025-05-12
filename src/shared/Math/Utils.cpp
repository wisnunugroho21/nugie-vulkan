#include "Utils.h"

glm::vec2 clampLength(const glm::vec2 &v, float maxLength) {
	const float l = length(v);
	return (l > maxLength) ? normalize(v) * maxLength : v;
}

float random01() {
	return (float)rand() / (float)RAND_MAX;
}

float randomFloat(float min, float max) {
	return min + (max - min) * random01();
}

glm::vec3 randomVec(const glm::vec3 &min, const glm::vec3 &max) {
	return vec3(randomFloat(min.x, max.x), randomFloat(min.y, max.y), randomFloat(min.z, max.z));
}

glm::vec3 randVec() {
	return randomVec(glm::vec3(-5, -5, -5), glm::vec3(5, 5, 5));
}

void getFrustumPlanes(glm::mat4 viewProj, glm::vec4 *planes) {
	viewProj = glm::transpose(viewProj);
	planes[0] = glm::vec4(viewProj[3] + viewProj[0]); // left
	planes[1] = glm::vec4(viewProj[3] - viewProj[0]); // right
	planes[2] = glm::vec4(viewProj[3] + viewProj[1]); // bottom
	planes[3] = glm::vec4(viewProj[3] - viewProj[1]); // top
	planes[4] = glm::vec4(viewProj[3] + viewProj[2]); // near
	planes[5] = glm::vec4(viewProj[3] - viewProj[2]); // far
}

void getFrustumCorners(glm::mat4 viewProj, glm::vec4 *points) {
	const glm::vec4 corners[] = {vec4(-1, -1, -1, 1), glm::vec4(1, -1, -1, 1), glm::vec4(1, 1, -1, 1), glm::vec4(-1, 1, -1, 1),
							vec4(-1, -1, 1, 1), glm::vec4(1, -1, 1, 1), glm::vec4(1, 1, 1, 1), glm::vec4(-1, 1, 1, 1)};

	const glm::mat4 invViewProj = glm::inverse(viewProj);

	for (int i = 0; i != 8; i++) {
		const glm::vec4 q = invViewProj * corners[i];
		points[i] = q / q.w;
	}
}