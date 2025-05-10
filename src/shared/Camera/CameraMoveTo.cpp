#include "Camera.h"

void CameraPositioner_MoveTo::update(float deltaSeconds, const glm::vec2 &mousePos, bool mousePressed) {
    positionCurrent_ += dampingLinear_ * deltaSeconds * (positionDesired_ - positionCurrent_);

    // normalization is required to avoid "spinning" around the object 2pi times
    anglesCurrent_ = clipAngles(anglesCurrent_);
    anglesDesired_ = clipAngles(anglesDesired_);

    // update angles
    anglesCurrent_ -= angleDelta(anglesCurrent_, anglesDesired_) * dampingEulerAngles_ * deltaSeconds;

    // normalize new angles
    anglesCurrent_ = clipAngles(anglesCurrent_);

    const glm::vec3 a = glm::radians(anglesCurrent_);

    currentTransform_ = glm::translate(glm::yawPitchRoll(a.y, a.x, a.z), -positionCurrent_);
}

float CameraPositioner_MoveTo::clipAngle(float d) {
    if (d < -180.0f)
        return d + 360.0f;
    if (d > +180.0f)
        return d - 360.f;
    return d;
}

glm::vec3 CameraPositioner_MoveTo::clipAngles(const glm::vec3 &angles) {
    return glm::vec3(
        std::fmod(angles.x, 360.0f),
        std::fmod(angles.y, 360.0f),
        std::fmod(angles.z, 360.0f));
}

glm::vec3 CameraPositioner_MoveTo::angleDelta(const glm::vec3 &anglesCurrent, const glm::vec3 &anglesDesired) {
    const glm::vec3 d = clipAngles(anglesCurrent) - clipAngles(anglesDesired);
    return glm::vec3(clipAngle(d.x), clipAngle(d.y), clipAngle(d.z));
}