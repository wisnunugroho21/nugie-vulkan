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

struct BoundingBox {
public:
	vec3 min_;
	vec3 max_;

    BoundingBox() = default;	
	BoundingBox(const vec3 &min, const vec3 &max);
	BoundingBox(const vec3 *points, size_t numPoints);    

	vec3 getSize() const;
	vec3 getCenter() const;
	BoundingBox getTransformed(const glm::mat4 &t) const;

	void transform(const glm::mat4 &t);
	void combinePoint(const vec3 &p);
	bool isInFrustum(glm::vec4 *frustumPlanes, glm::vec4 *frustumCorners);

	static BoundingBox combineBoxes(const std::vector<BoundingBox> &boxes);
};
