#pragma once

#define _USE_MATH_DEFINES
#include <cmath>
#include <glm/ext.hpp>
#include <glm/glm.hpp>

#include <vector>

using glm::mat4;
using glm::vec2;
using glm::vec3;
using glm::vec4;

namespace Math {
	static constexpr float PI = 3.14159265359f;
	static constexpr float TWOPI = 6.28318530718f;
} // namespace Math

template <typename T>
T clamp(T v, T a, T b) {
	if (v < a)
		return a;
	if (v > b)
		return b;

	return v;
}

vec2 clampLength(const vec2 &v, float maxLength);

float random01();
float randomFloat(float min, float max);
vec3 randomVec(const vec3 &min, const vec3 &max);
vec3 randVec();

void getFrustumPlanes(mat4 viewProj, vec4 *planes);
void getFrustumCorners(mat4 viewProj, vec4 *points);
