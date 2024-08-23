
// Windows dependencies
#include <windows.h>
#include <dwmapi.h> // For dark-mode support
#pragma comment(lib, "dwmapi.lib")

// Std dependencies
#include <iostream>
#include <vector>

// Third party dependencies
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>
#include <nfd.hpp>

// Internal dependencies
#include <system/windowPicker.h>
#include <system/mouseTracker.h>
#include <system/windowCaptureService.h>

#include <utils.h>

#include <bot/ibotwindow.h>
#include <bot/botManagerWindow.h>
#include <bot/trainingLabWindow.h>

// Error callback for GLFW
void glfwErrorCallback(int error, const char* description)
{
    fprintf(stdout, "GLFW Error %d: %s\n", error, description);
}

void setDarkMode(GLFWwindow* window)
{
	HWND hwnd = glfwGetWin32Window(window);
	if (!hwnd) return;

	// Set dark mode
	BOOL useDarkMode = true;
	DwmSetWindowAttribute(hwnd, DWMWINDOWATTRIBUTE::DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode));
}

int main(int argc, char** argv)
{
    // Set error callback
    glfwSetErrorCallback(glfwErrorCallback);

    // Initialize GLFW
    if (!glfwInit())
        return -1;

    // Set GLFW window hints
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	std::cout << "Picking a monitor to track...\n";
	std::flush(std::cout);
    auto [hdc, trackingMonitor] = pickMonitorDialog();
    if (!hdc)
	{
		std::cout << "ERROR! No monitor detected!\n";
		return -1;
	}

	std::cout << "Starting capture service...\n";
	std::flush(std::cout);
	WindowCaptureService& captureService = WindowCaptureService::getInstance();
	captureService.StartCapture(hdc);

    // Create a windowed mode window and its OpenGL context
	glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
    GLFWwindow* window = glfwCreateWindow(1920, 1080, "OSRS Machine-Learning Bot", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

	// Move window to non-tracking monitor
	{
		GLFWmonitor* nonTrackingMonitor = glfwGetPrimaryMonitor();
		int monitorCount;
		GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
		for (int i = 0; i < monitorCount; i++)
		{
			if (monitors[i] != trackingMonitor)
			{
				nonTrackingMonitor = monitors[i];
				break;
			}
		}

		// Fetch monitor position
		int xpos, ypos;
		glfwGetMonitorPos(nonTrackingMonitor, &xpos, &ypos);
		glfwSetWindowPos(window, xpos, ypos);
	}

    // Make the window's context current
    glfwMakeContextCurrent(window);

    // Initialize GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        fprintf(stderr, "Failed to initialize GLAD\n");
        return -1;
    }

	// Clear and swap buffers once to prevent white screen
	glClearColor(0.0f, 0.0f, 0.0f, 1.00f);
	glClear(GL_COLOR_BUFFER_BIT);
	glfwSwapBuffers(window);

	// Set window initial properties
	setDarkMode(window);
    glfwSwapInterval(1); // Enable vsync
	glfwMaximizeWindow(window);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable Docking

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 440");

	// Allocate app windows
	std::vector<IBotWindow*> botWindows = {
		new TrainingLabWindow(),
		new BotManagerWindow(),
	};

    while (!glfwWindowShouldClose(window))
    {
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Poll and handle events (inputs, window resize, etc.)
        glfwPollEvents();

        // Start the ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::DockSpaceOverViewport();

		// ================ //
		// LOGIC LOOP START //
		// ================ //

		// Draw each window
		for (auto& botWindow : botWindows)
		{
			botWindow->Run();
		}

		// ============== //
		// LOGIC LOOP END //
		// ============== //

        // Render
        ImGui::Render();
        int displayW, displayH;
        glfwGetFramebufferSize(window, &displayW, &displayH);
        glViewport(0, 0, displayW, displayH);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Swap buffers
        glfwSwapBuffers(window);
    }

	for (auto& botWindow : botWindows)
	{
		delete botWindow;
	}

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

	// Stop capturing before deleting window handle
	captureService.StopCapture();

	DeleteDC(hdc);

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}