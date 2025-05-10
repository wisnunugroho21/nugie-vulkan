#pragma once

#include <assert.h>
#include <algorithm>

#include "shared/Math/Utils.h"
#include "shared/Camera/Trackball.h"

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/euler_angles.hpp"

class CameraPositionerInterface {
public:
	virtual ~CameraPositionerInterface() = default;
	virtual glm::mat4 getViewMatrix() const = 0;
	virtual glm::vec3 getPosition() const = 0;
};

class Camera final {
public:
	explicit Camera(CameraPositionerInterface &positioner)
		: positioner_(&positioner)
	{
	}

	Camera(const Camera &) = default;
	Camera &operator=(const Camera &) = default;

	glm::mat4 getViewMatrix() const { return positioner_->getViewMatrix(); }
	glm::vec3 getPosition() const { return positioner_->getPosition(); }
	glm::mat4 getProjMatrix() const { return proj_; }

private:
	const CameraPositionerInterface *positioner_;
	glm::mat4 proj_;
};

class CameraPositioner_FirstPerson final : public CameraPositionerInterface {
public:
	CameraPositioner_FirstPerson() = default;
	CameraPositioner_FirstPerson(const glm::vec3 &pos, const glm::vec3 &target, const glm::vec3 &up)
		: cameraPosition_(pos), cameraOrientation_(glm::lookAt(pos, target, up)), up_(up)
	{
	}	

	virtual glm::mat4 getViewMatrix() const override;

	virtual glm::vec3 getPosition() const override;

	void update(double deltaSeconds, const glm::vec2 &mousePos, bool mousePressed);

	void setPosition(const glm::vec3 &pos);
	void setSpeed(const glm::vec3 &speed);
	void setUpVector(const glm::vec3 &up);

	void resetMousePosition(const glm::vec2 &p);
	void lookAt(const glm::vec3 &pos, const glm::vec3 &target, const glm::vec3 &up);

public:
	struct Movement {
		bool forward_ = false;
		bool backward_ = false;
		bool left_ = false;
		bool right_ = false;
		bool up_ = false;
		bool down_ = false;
		//
		bool fastSpeed_ = false;
	} movement_;

public:
	float mouseSpeed_ = 4.0f;
	float acceleration_ = 150.0f;
	float damping_ = 0.2f;
	float maxSpeed_ = 10.0f;
	float fastCoef_ = 10.0f;

private:
	glm::vec2 mousePos_ = glm::vec2(0);
	glm::vec3 cameraPosition_ = glm::vec3(0.0f, 10.0f, 10.0f);
	glm::quat cameraOrientation_ = glm::quat(glm::vec3(0));
	glm::vec3 moveSpeed_ = glm::vec3(0.0f);
	glm::vec3 up_ = glm::vec3(0.0f, 0.0f, 1.0f);
};

class CameraPositioner_MoveTo final : public CameraPositionerInterface {
public:
	CameraPositioner_MoveTo(const glm::vec3 &pos, const glm::vec3 &angles)
		: positionCurrent_(pos), positionDesired_(pos), anglesCurrent_(angles), anglesDesired_(angles)
	{
	}

	void update(float deltaSeconds, const glm::vec2 &mousePos, bool mousePressed);

	void setPosition(const glm::vec3 &p) { positionCurrent_ = p; }
	void setAngles(float pitch, float pan, float roll) { anglesCurrent_ = glm::vec3(pitch, pan, roll); }
	void setAngles(const glm::vec3 &angles) { anglesCurrent_ = angles; }
	void setDesiredPosition(const glm::vec3 &p) { positionDesired_ = p; }
	void setDesiredAngles(float pitch, float pan, float roll) { anglesDesired_ = glm::vec3(pitch, pan, roll); }
	void setDesiredAngles(const glm::vec3 &angles) { anglesDesired_ = angles; }

	virtual glm::vec3 getPosition() const override { return positionCurrent_; }
	virtual glm::mat4 getViewMatrix() const override { return currentTransform_; }

public:
	float dampingLinear_ = 10.0f;
	glm::vec3 dampingEulerAngles_ = glm::vec3(5.0f, 5.0f, 5.0f);

private:
	glm::vec3 positionCurrent_ = glm::vec3(0.0f);
	glm::vec3 positionDesired_ = glm::vec3(0.0f);

	/// pitch, pan, roll
	glm::vec3 anglesCurrent_ = glm::vec3(0.0f);
	glm::vec3 anglesDesired_ = glm::vec3(0.0f);

	glm::mat4 currentTransform_ = glm::mat4(1.0f);	

	static glm::vec3 clipAngles(const glm::vec3 &angles);
	static glm::vec3 angleDelta(const glm::vec3 &anglesCurrent, const glm::vec3 &anglesDesired);

	static float clipAngle(float d);
};
