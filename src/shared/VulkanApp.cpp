#include "VulkanApp.h"

#include <unordered_map>

extern std::unordered_map<uint32_t, std::string> debugGLSLSourceCode;

static void shaderModuleCallback(lvk::IContext *, lvk::ShaderModuleHandle handle, int line, int col, const char *debugName) {
	const auto it = debugGLSLSourceCode.find(handle.index());

	if (it != debugGLSLSourceCode.end()) {
		lvk::logShaderSource(it->second.c_str());
	}
}

VulkanApp::VulkanApp(const VulkanAppConfig &cfg) : cfg_(cfg) {
	ImGui::CreateContext();
	minilog::initialize(nullptr, {.threadNames = false});

	int width = 1280;
	int height = 720;

	if (cfg_.contextConfig.shaderModuleErrorCallback == nullptr) {
		cfg_.contextConfig.shaderModuleErrorCallback = &shaderModuleCallback;
	}

	window_ = lvk::initWindow("Simple example", width, height);
	ctx_ = lvk::createVulkanContextWithSwapchain(window_, width, height, cfg_.contextConfig);

	glfwSetWindowUserPointer(window_, this);
	glfwSetMouseButtonCallback(window_, 
		[](GLFWwindow *window, int button, int action, int mods) {
			VulkanApp* app = (VulkanApp*)glfwGetWindowUserPointer(window);
			if (button == GLFW_MOUSE_BUTTON_LEFT) {
			app->mouseState_.pressedLeft = action == GLFW_PRESS;
			}

			double xpos, ypos;
			glfwGetCursorPos(window, &xpos, &ypos);
			const ImGuiMouseButton_ imguiButton = (button == GLFW_MOUSE_BUTTON_LEFT)
												? ImGuiMouseButton_Left
												: (button == GLFW_MOUSE_BUTTON_RIGHT ? ImGuiMouseButton_Right : ImGuiMouseButton_Middle);

			ImGuiIO& io                         = ImGui::GetIO();
			io.MousePos                         = ImVec2((float)xpos, (float)ypos);
			io.MouseDown[imguiButton]           = action == GLFW_PRESS;

			for (auto& cb : app->callbacksMouseButton) {
				cb(window, button, action, mods);
			} 
		}
	);

	glfwSetScrollCallback(window_, 
		[](GLFWwindow *window, double dx, double dy) {
			ImGuiIO& io    = ImGui::GetIO();
			io.MouseWheelH = (float)dx;
			io.MouseWheel  = (float)dy; 
		}
	);

	glfwSetCursorPosCallback(window_, 
		[](GLFWwindow *window, double x, double y) {
			VulkanApp* app = (VulkanApp*)glfwGetWindowUserPointer(window);

			int width, height;
			glfwGetFramebufferSize(window, &width, &height);

			ImGui::GetIO().MousePos = ImVec2(x, y);
			app->mouseState_.pos.x  = static_cast<float>(x / width);
			app->mouseState_.pos.y  = 1.0f - static_cast<float>(y / height); 
		});

	glfwSetKeyCallback(window_, 
		[](GLFWwindow *window, int key, int scancode, int action, int mods) {
			VulkanApp* app     = (VulkanApp*)glfwGetWindowUserPointer(window);
			const bool pressed = action != GLFW_RELEASE;

			if (key == GLFW_KEY_ESCAPE && pressed)
				glfwSetWindowShouldClose(window, GLFW_TRUE);
			if (key == GLFW_KEY_W)
				app->positioner_.movement_.forward_ = pressed;
			if (key == GLFW_KEY_S)
				app->positioner_.movement_.backward_ = pressed;
			if (key == GLFW_KEY_A)
				app->positioner_.movement_.left_ = pressed;
			if (key == GLFW_KEY_D)
				app->positioner_.movement_.right_ = pressed;
			if (key == GLFW_KEY_Q)
				app->positioner_.movement_.up_ = pressed;
			if (key == GLFW_KEY_E)
				app->positioner_.movement_.down_ = pressed;

			app->positioner_.movement_.fastSpeed_ = (mods & GLFW_MOD_SHIFT) != 0;

			if (key == GLFW_KEY_SPACE) {
				app->positioner_.lookAt(app->cfg_.initialCameraPos, app->cfg_.initialCameraTarget, vec3(0.0f, 1.0f, 0.0f));
				app->positioner_.setSpeed(vec3(0));
			}

			for (auto& cb : app->callbacksKey) {
				cb(window, key, scancode, action, mods);
			}
		}
	);
}

VulkanApp::~VulkanApp() {
	ctx_ = nullptr;
	ImGui::DestroyContext();

	glfwDestroyWindow(window_);
	glfwTerminate();
}

void VulkanApp::run(DrawFrameFunc drawFrame) {
	double timeStamp = glfwGetTime();
	float deltaSeconds = 0.0f;

	while (!glfwWindowShouldClose(window_)) {
		fpsCounter_.tick(deltaSeconds);
		const double newTimeStamp = glfwGetTime();
		deltaSeconds = static_cast<float>(newTimeStamp - timeStamp);
		timeStamp = newTimeStamp;

		glfwPollEvents();
		int width, height;
#if defined(__APPLE__)
		// a hacky workaround for retina displays
		glfwGetWindowSize(window_, &width, &height);
#else
		glfwGetFramebufferSize(window_, &width, &height);
#endif
		if (!width || !height)
			continue;
		const float ratio = width / (float)height;

		positioner_.update(deltaSeconds, mouseState_.pos, ImGui::GetIO().WantCaptureMouse ? false : mouseState_.pressedLeft);
		projection_.setAspectRatio(ratio);

		drawFrame((uint32_t)width, (uint32_t)height, deltaSeconds);
	}
}
