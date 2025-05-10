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
#include "shared/Cubemap/Bitmap.h"
#include "shared/Cubemap/Utils.h"

#include <functional>

using DrawFrameFunc = std::function<void(uint32_t width, uint32_t height, float aspectRatio, float deltaSeconds)>;

struct GLTFMaterialIntro {
	std::string name;
	uint32_t materialMask;
	uint32_t currentMaterialMask;
	bool modified = false;
};

struct GLTFIntrospective {
	std::vector<std::string> cameras;
	uint32_t activeCamera = ~0u;

	std::vector<std::string> animations;
	std::vector<uint32_t> activeAnim;

	std::vector<std::string> extensions;
	std::vector<uint32_t> activeExtension;

	bool showCameras = false;
};

struct VulkanAppConfig {
	vec3 initialCameraPos = vec3(0.0f, 0.0f, -2.5f);
	vec3 initialCameraTarget = vec3(0.0f, 0.0f, 0.0f);
	bool showGLTFInspector = false;
	lvk::ContextConfig contextConfig = {};
};

class VulkanApp {
public:
	explicit VulkanApp(const VulkanAppConfig &cfg = {});
	virtual ~VulkanApp();

	virtual void run(DrawFrameFunc drawFrame);

	virtual void drawGrid(
		lvk::ICommandBuffer &buf, const mat4 &proj, const vec3 &origin = vec3(0.0f), uint32_t numSamples = 1,
		lvk::Format colorFormat = lvk::Format_Invalid);
	virtual void drawGrid(
		lvk::ICommandBuffer &buf, const mat4 &mvp, const vec3 &origin, const vec3 &camPos, uint32_t numSamples = 1,
		lvk::Format colorFormat = lvk::Format_Invalid);

	virtual void drawFPS();
	virtual void drawMemo();
	virtual void drawGTFInspector(GLTFIntrospective &intro);
	virtual void drawGTFInspector_Cameras(GLTFIntrospective &intro);

	lvk::Format getDepthFormat() const;
	lvk::TextureHandle getDepthTexture() const { return depthTexture_; }

	void addMouseButtonCallback(GLFWmousebuttonfun cb) { callbacksMouseButton.push_back(cb); }
	void addKeyCallback(GLFWkeyfun cb) { callbacksKey.push_back(cb); }

public:
	GLFWwindow *window_ = nullptr;
	std::unique_ptr<lvk::IContext> ctx_;	
	lvk::Holder<lvk::TextureHandle> depthTexture_;

	FramesPerSecondCounter fpsCounter_ = FramesPerSecondCounter(0.5f);
	std::unique_ptr<lvk::ImGuiRenderer> imgui_;

	VulkanAppConfig cfg_{};

	CameraPositioner_FirstPerson positioner_{cfg_.initialCameraPos, cfg_.initialCameraTarget, vec3(0.0f, 1.0f, 0.0f)};
	Camera camera_{positioner_};

	struct MouseState {
		vec2 pos = vec2(0.0f);
		bool pressedLeft = false;
	} mouseState_;

protected:
	lvk::Holder<lvk::ShaderModuleHandle> gridVert{};
	lvk::Holder<lvk::ShaderModuleHandle> gridFrag{};
	lvk::Holder<lvk::RenderPipelineHandle> gridPipeline{};

	uint32_t pipelineSamples = 1;

	std::vector<GLFWmousebuttonfun> callbacksMouseButton;
	std::vector<GLFWkeyfun> callbacksKey;
};
