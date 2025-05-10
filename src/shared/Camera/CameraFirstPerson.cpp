#include "Camera.h"

void CameraPositioner_FirstPerson::update(double deltaSeconds, const glm::vec2 &mousePos, bool mousePressed) {
    if (mousePressed) {
        const glm::vec2 delta = mousePos - mousePos_;
        const glm::quat deltaQuat = glm::quat(glm::vec3(-mouseSpeed_ * delta.y, mouseSpeed_ * delta.x, 0.0f));
        cameraOrientation_ = deltaQuat * cameraOrientation_;
        cameraOrientation_ = glm::normalize(cameraOrientation_);
        setUpVector(up_);
    }

    mousePos_ = mousePos;

    const glm::mat4 v = glm::mat4_cast(cameraOrientation_);

    const glm::vec3 forward = -glm::vec3(v[0][2], v[1][2], v[2][2]);
    const glm::vec3 right = glm::vec3(v[0][0], v[1][0], v[2][0]);
    const glm::vec3 up = glm::cross(right, forward);

    glm::vec3 accel(0.0f);

    if (movement_.forward_)
        accel += forward;
    if (movement_.backward_)
        accel -= forward;

    if (movement_.left_)
        accel -= right;
    if (movement_.right_)
        accel += right;

    if (movement_.up_)
        accel += up;
    if (movement_.down_)
        accel -= up;

    if (movement_.fastSpeed_)
        accel *= fastCoef_;

    if (accel == glm::vec3(0)) {
        // decelerate naturally according to the damping value
        moveSpeed_ -= moveSpeed_ * std::min((1.0f / damping_) * static_cast<float>(deltaSeconds), 1.0f);
    } else {
        // acceleration
        moveSpeed_ += accel * acceleration_ * static_cast<float>(deltaSeconds);
        const float maxSpeed = movement_.fastSpeed_ ? maxSpeed_ * fastCoef_ : maxSpeed_;
        if (glm::length(moveSpeed_) > maxSpeed)
            moveSpeed_ = glm::normalize(moveSpeed_) * maxSpeed;
    }

    cameraPosition_ += moveSpeed_ * static_cast<float>(deltaSeconds);
}

glm::mat4 CameraPositioner_FirstPerson::getViewMatrix() const {
    const glm::mat4 t = glm::translate(glm::mat4(1.0f), -cameraPosition_);
    const glm::mat4 r = glm::mat4_cast(cameraOrientation_);
    return r * t;
}

glm::vec3 CameraPositioner_FirstPerson::getPosition() const {
    return cameraPosition_;
}

void CameraPositioner_FirstPerson::setPosition(const glm::vec3 &pos) {
    cameraPosition_ = pos;
}

void CameraPositioner_FirstPerson::setSpeed(const glm::vec3 &speed) {
    moveSpeed_ = speed;
}

void CameraPositioner_FirstPerson::resetMousePosition(const glm::vec2 &p) { 
    mousePos_ = p; 
};

void CameraPositioner_FirstPerson::setUpVector(const glm::vec3 &up) {
    const glm::mat4 view = getViewMatrix();
    const glm::vec3 dir = -glm::vec3(view[0][2], view[1][2], view[2][2]);
    cameraOrientation_ = glm::lookAt(cameraPosition_, cameraPosition_ + dir, up);
}

void CameraPositioner_FirstPerson::lookAt(const glm::vec3 &pos, const glm::vec3 &target, const glm::vec3 &up) {
    cameraPosition_ = pos;
    cameraOrientation_ = glm::lookAt(pos, target, up);
}