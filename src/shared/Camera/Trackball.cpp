#include "Trackball.h"

glm::mat4 VirtualTrackball::dragTo(glm::vec2 screenPoint, float speed, bool keyPressed) {
    if (keyPressed && !isDraggingActive_) {
        startDragging(screenPoint);
        isDraggingActive_ = keyPressed;

        return glm::mat4(1.0f);
    }

    isDraggingActive_ = keyPressed;

    if (!keyPressed)
        return glm::mat4(1.0f);

    pointCur_ = projectOnSphere(screenPoint);

    const glm::vec3 direction = pointCur_ - pointPrev_;
    const float shift = glm::length(direction);

    glm::mat4 rotMatrix = glm::mat4(1.0f);

    if (shift > std::numeric_limits<float>::epsilon()) {
        const glm::vec3 axis = glm::cross(pointPrev_, pointCur_);
        rotMatrix = glm::rotate(glm::mat4(1.0f), shift * speed, axis);
    }

    rotationDelta_ = rotMatrix;
    return rotMatrix;
}

const glm::mat4& VirtualTrackball::getRotationDelta() const {
    return rotationDelta_;
}

/**
    Get current rotation matrix
**/
glm::mat4 VirtualTrackball::getRotationMatrix() const {
    return rotation_ * rotationDelta_;
}

void VirtualTrackball::startDragging(glm::vec2 screenPoint) {
    rotation_ = rotation_ * rotationDelta_;
    rotationDelta_ = glm::mat4(1.0f);
    pointCur_ = projectOnSphere(screenPoint);
    pointPrev_ = pointCur_;
}

glm::vec3 VirtualTrackball::projectOnSphere(glm::vec2 ScreenPoint) {
    // convert to -1.0...1.0 range
    glm::vec3 proj(
        +(2.0f * ScreenPoint.x - 1.0f),
        -(2.0f * ScreenPoint.y - 1.0f),
        0.0f
    );

    const float Length = std::min(glm::length(proj), 1.0f);
    proj.z = sqrtf(1.001f - Length * Length);

    return glm::normalize(proj);
}