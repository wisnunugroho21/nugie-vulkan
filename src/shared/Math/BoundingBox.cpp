#include "BoundingBox.h"

bool BoundingBox::isInFrustum(glm::vec4 *frustumPlanes, glm::vec4 *frustumCorners) {
	using glm::dot;

	for (int i = 0; i < 6; i++) {
		int r = 0;

		r += (dot(frustumPlanes[i], glm::vec4(min_.x, min_.y, min_.z, 1.0f)) < 0.0) ? 1 : 0;
		r += (dot(frustumPlanes[i], glm::vec4(max_.x, min_.y, min_.z, 1.0f)) < 0.0) ? 1 : 0;
		r += (dot(frustumPlanes[i], glm::vec4(min_.x, max_.y, min_.z, 1.0f)) < 0.0) ? 1 : 0;
		r += (dot(frustumPlanes[i], glm::vec4(max_.x, max_.y, min_.z, 1.0f)) < 0.0) ? 1 : 0;
		r += (dot(frustumPlanes[i], glm::vec4(min_.x, min_.y, max_.z, 1.0f)) < 0.0) ? 1 : 0;
		r += (dot(frustumPlanes[i], glm::vec4(max_.x, min_.y, max_.z, 1.0f)) < 0.0) ? 1 : 0;
		r += (dot(frustumPlanes[i], glm::vec4(min_.x, max_.y, max_.z, 1.0f)) < 0.0) ? 1 : 0;
		r += (dot(frustumPlanes[i], glm::vec4(max_.x, max_.y, max_.z, 1.0f)) < 0.0) ? 1 : 0;

		if (r == 8)
			return false;
	}

	// check frustum outside/inside box
	int r = 0;
	for (int i = 0; i < 8; i++)
		r += ((frustumCorners[i].x > max_.x) ? 1 : 0);
	if (r == 8)
		return false;

	r = 0;
	for (int i = 0; i < 8; i++)
		r += ((frustumCorners[i].x < min_.x) ? 1 : 0);
	if (r == 8)
		return false;

	r = 0;
	for (int i = 0; i < 8; i++)
		r += ((frustumCorners[i].y > max_.y) ? 1 : 0);
	if (r == 8)
		return false;

	r = 0;
	for (int i = 0; i < 8; i++)
		r += ((frustumCorners[i].y < min_.y) ? 1 : 0);
	if (r == 8)
		return false;

	r = 0;
	for (int i = 0; i < 8; i++)
		r += ((frustumCorners[i].z > max_.z) ? 1 : 0);
	if (r == 8)
		return false;

	r = 0;
	for (int i = 0; i < 8; i++)
		r += ((frustumCorners[i].z < min_.z) ? 1 : 0);
	if (r == 8)
		return false;

	return true;
}

BoundingBox::BoundingBox(const glm::vec3 &min, const glm::vec3 &max)
		: min_(glm::min(min, max)), max_(glm::max(min, max))
{
}

BoundingBox::BoundingBox(const glm::vec3 *points, size_t numPoints) {
    glm::vec3 vmin(std::numeric_limits<float>::max());
    glm::vec3 vmax(std::numeric_limits<float>::lowest());

    for (size_t i = 0; i != numPoints; i++) {
        vmin = glm::min(vmin, points[i]);
        vmax = glm::max(vmax, points[i]);
    }

    min_ = vmin;
    max_ = vmax;
}

glm::vec3 BoundingBox::getSize() const { 
    return glm::vec3(max_[0] - min_[0], max_[1] - min_[1], max_[2] - min_[2]); 
}

glm::vec3 BoundingBox::getCenter() const { 
    return 0.5f * glm::vec3(max_[0] + min_[0], max_[1] + min_[1], max_[2] + min_[2]); 
}

void BoundingBox::transform(const glm::mat4 &t) {
    glm::vec3 corners[] = {
        glm::vec3(min_.x, min_.y, min_.z),
        glm::vec3(min_.x, max_.y, min_.z),
        glm::vec3(min_.x, min_.y, max_.z),
        glm::vec3(min_.x, max_.y, max_.z),
        glm::vec3(max_.x, min_.y, min_.z),
        glm::vec3(max_.x, max_.y, min_.z),
        glm::vec3(max_.x, min_.y, max_.z),
        glm::vec3(max_.x, max_.y, max_.z),
    };

    for (auto &v : corners)
        v = glm::vec3(t * glm::vec4(v, 1.0f));

    *this = BoundingBox(corners, 8);
}

BoundingBox BoundingBox::getTransformed(const glm::mat4 &t) const {
    BoundingBox b = *this;
    b.transform(t);
    return b;
}

void BoundingBox::combinePoint(const glm::vec3 &p) {
    min_ = glm::min(min_, p);
    max_ = glm::max(max_, p);
}

BoundingBox BoundingBox::combineBoxes(const std::vector<BoundingBox> &boxes) {
    std::vector<glm::vec3> allPoints;
	allPoints.reserve(boxes.size() * 8);

	for (const auto &b : boxes) {
		allPoints.emplace_back(b.min_.x, b.min_.y, b.min_.z);
		allPoints.emplace_back(b.min_.x, b.min_.y, b.max_.z);
		allPoints.emplace_back(b.min_.x, b.max_.y, b.min_.z);
		allPoints.emplace_back(b.min_.x, b.max_.y, b.max_.z);

		allPoints.emplace_back(b.max_.x, b.min_.y, b.min_.z);
		allPoints.emplace_back(b.max_.x, b.min_.y, b.max_.z);
		allPoints.emplace_back(b.max_.x, b.max_.y, b.min_.z);
		allPoints.emplace_back(b.max_.x, b.max_.y, b.max_.z);
	}

	return BoundingBox(allPoints.data(), allPoints.size());
}