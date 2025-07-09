#pragma once

#include <lvk/HelpersImGui.h>
#include <lvk/LVK.h>

#include <GLFW/glfw3.h>
#include <implot/implot.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/ext.hpp>
#include <glm/glm.hpp>

#include <stb_image.h>
#include <stb_image_write.h>

#include "shared/Math/FramesPerSecondCounter.h"
#include "shared/Camera/Camera.h"
#include "shared/Util/Utils.h"

#include <functional>

using DrawFrameFunc = std::function<void(uint32_t width, uint32_t height, float deltaSeconds)>;

struct VulkanAppConfig {
	vec3 initialCameraPos = vec3(0.0f, 0.0f, -2.5f);
	vec3 initialCameraTarget = vec3(0.0f, 0.0f, 0.0f);
	float fovy = 45.0f;
	float near = 0.1f; 
	float far = 1000.0f;
	bool showGLTFInspector = false;
	lvk::ContextConfig contextConfig = {};
};

class VulkanApp {
public:
	explicit VulkanApp(const VulkanAppConfig &cfg = {});
	virtual ~VulkanApp();

	virtual void run(DrawFrameFunc drawFrame);

	void addMouseButtonCallback(GLFWmousebuttonfun cb) { callbacksMouseButton.push_back(cb); }
	void addKeyCallback(GLFWkeyfun cb) { callbacksKey.push_back(cb); }

public:
	VulkanAppConfig cfg_{};

	GLFWwindow *window_ = nullptr;
	std::unique_ptr<lvk::IContext> ctx_;

	FramesPerSecondCounter fpsCounter_{0.5f};

	CameraPositioner_FirstPerson positioner_{cfg_.initialCameraPos, cfg_.initialCameraTarget, vec3(0.0f, 1.0f, 0.0f)};
	CameraProjection_Perspective projection_{cfg_.fovy, 0.0f, cfg_.near, cfg_.far};
	Camera camera_{&positioner_, &projection_};

	struct MouseState {
		vec2 pos = vec2(0.0f);
		bool pressedLeft = false;
	} mouseState_;

protected:
	std::vector<GLFWmousebuttonfun> callbacksMouseButton;
	std::vector<GLFWkeyfun> callbacksKey;
};
