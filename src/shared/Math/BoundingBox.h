#pragma once

#define _USE_MATH_DEFINES
#include <cmath>
#include <glm/ext.hpp>
#include <glm/glm.hpp>

#include <vector>

struct BoundingBox {
public:
	glm::vec3 min_;
	glm::vec3 max_;

    BoundingBox() = default;	
	BoundingBox(const glm::vec3 &min, const glm::vec3 &max);
	BoundingBox(const glm::vec3 *points, size_t numPoints);    

	glm::vec3 getSize() const;
	glm::vec3 getCenter() const;
	BoundingBox getTransformed(const glm::mat4 &t) const;

	void transform(const glm::mat4 &t);
	void combinePoint(const glm::vec3 &p);
	bool isInFrustum(glm::vec4 *frustumPlanes, glm::vec4 *frustumCorners);

	static BoundingBox combineBoxes(const std::vector<BoundingBox> &boxes);
};
