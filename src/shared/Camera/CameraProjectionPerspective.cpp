#include "shared/Camera/Camera.h"

CameraProjection_Perspective::CameraProjection_Perspective(float fovy, float aspect, float near, float far) 
    : fovy_{fovy}, aspectRatio_{aspect}, near_{near}, far_{far}
{
}

void CameraProjection_Perspective::setAspectRatio(float aspectRatio) {
    aspectRatio_ = aspectRatio;
}

void CameraProjection_Perspective::setFovy(float fovy) {
    fovy_ = fovy;
}

glm::mat4 CameraProjection_Perspective::getProjectionMatrix() const {
    return glm::perspective(fovy_, aspectRatio_, near_, far_);
}