
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


// Internal dependencies
#include <windowPicker.h>

#include <mouseTracker.h>
#include <windowCapture.h>
#include <onnxruntimeInference.h>

// Error callback for GLFW
void glfwErrorCallback(int error, const char* description)
{
    fprintf(stdout, "GLFW Error %d: %s\n", error, description);
}

void drawWindowTitle(const char* title, ImVec2 windowPos)
{
	// Hold cursor pos to restore later
	ImVec2 cursorPos = ImGui::GetCursorScreenPos();

	// Draw text with border on top of the child window
	ImVec2 textPos = ImVec2(windowPos.x + 5, windowPos.y - 7);
	ImGui::SetCursorScreenPos(textPos);
	ImGui::TextUnformatted(title);

	// Get the draw list and draw the border around the text
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	ImVec2 textSize = ImGui::CalcTextSize(title);
	ImVec2 borderMin = ImVec2(textPos.x - 3, textPos.y - 2);
	ImVec2 borderMax = ImVec2(textPos.x + textSize.x + 2, textPos.y + textSize.y + 2);
	drawList->AddRect(borderMin, borderMax, ImGui::GetColorU32(ImGuiCol_Border));

	// Restore cursor pos
	ImGui::SetCursorScreenPos(cursorPos);
}

void setDarkMode(GLFWwindow* window)
{
	HWND hwnd = glfwGetWin32Window(window);
	if (!hwnd) return;

	// Set dark mode
	BOOL useDarkMode = true;
	DwmSetWindowAttribute(hwnd, DWMWINDOWATTRIBUTE::DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode));
}

void runBotInference(cv::Mat& image, YOLOv8& model, std::vector<YoloDetectionBox>& detections)
{
	// Run YOLO inference
	model.Inference(image, detections);

	// TODO: Draw using ImGui or OpenGL (for performance)
	// Draw rectangles on the image
	for (const auto& detection : detections)
	{
		cv::Rect rect(detection.x, detection.y, detection.w, detection.h);

		std::string label;
		cv::Scalar color = cv::Scalar(0, 0, 0);
		if (detection.classId == 0) { color = cv::Scalar(0, 128, 0);		label = "Adamant";	};
		if (detection.classId == 1) { color = cv::Scalar(79, 69, 54);		label = "Coal";		};
		if (detection.classId == 2) { color = cv::Scalar(51, 115, 184); 	label = "Copper";	};
		if (detection.classId == 3) { color = cv::Scalar(34, 34, 178);		label = "Iron";		};
		if (detection.classId == 4) { color = cv::Scalar(180, 130, 70); 	label = "Mithril";	};
		if (detection.classId == 5) { color = cv::Scalar(192, 192, 192);	label = "Silver";	};
		if (detection.classId == 6) { color = cv::Scalar(193, 205, 205);	label = "Tin";		};
		if (detection.classId == 7) { color = cv::Scalar(0, 0, 0);			label = "Wasted";	};
		cv::rectangle(image, rect, color, 2);
		cv::putText(image, label, rect.tl(), cv::FONT_HERSHEY_SIMPLEX, 0.5, color, 2);
	}
}

int main(int argc, char** argv)
{
	initializeCaptureApi();

	//Model loading
	constexpr auto modelPath = L"..\\..\\models\\yolov8s-osrs-ores-v5.onnx";
	std::wcout << "Initializing YOLOv8, from path '" << modelPath << "'\n";
	std::flush(std::wcout);

	// These params vary with the model
	YOLOv8 model(8 /*8 ore categories (subjected to change)*/, 0.85);
	model.LoadModel(true, modelPath);

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

    // Create a windowed mode window and its OpenGL context
	glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
    GLFWwindow* window = glfwCreateWindow(1920, 1080, "OSRS Machine-Learning Bot", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    // Make the window's context current
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync
	glfwMaximizeWindow(window);

	setDarkMode(window);

    // Initialize GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        fprintf(stderr, "Failed to initialize GLAD\n");
        return -1;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable Docking

    // Start the mouse tracker
    MouseTracker mouseTracker;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 440");

	// Clear and swap buffers once to prevent white screen
	glClearColor(0.0f, 0.0f, 0.0f, 1.00f);
	glClear(GL_COLOR_BUFFER_BIT);
	glfwSwapBuffers(window);

    // Generate a texture ID
    GLuint screenCaptureTex;
    glGenTextures(1, &screenCaptureTex);
    glBindTexture(GL_TEXTURE_2D, screenCaptureTex);

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Main loop
	cv::Mat image;
	std::vector<YoloDetectionBox> detections;
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

		// ================ //
		// LOGIC LOOP START //
		// ================ //

        // Create a docking space
        ImGui::DockSpaceOverViewport();

		// image loading
		captureScreen(hdc, image);
 		if (image.empty())
		{
			std::cout << "ERROR! blank image grabbed\n";
			return -2;
		}

		if (ImGui::Begin("Training"))
		{
			ImGui::SeparatorText("Welcome to the Training Lab!");

			int mousePosX, mousePosY;
			mouseTracker.GetMousePosition(mousePosX, mousePosY);
			int mouseDownX, mouseDownY;
			mouseTracker.GetMouseDownPosition(mouseDownX, mouseDownY);
			int mouseUpX, mouseUpY;
			mouseTracker.GetMouseUpPosition(mouseUpX, mouseUpY);

			ImGui::Text("Mouse position: (%i, %i)", mousePosX, mousePosY);
			ImGui::Text("Mouse down position: (%i, %i)", mouseDownX, mouseDownY);
			ImGui::Text("Mouse release position: (%i, %i)", mouseUpX, mouseUpY);

			ImGui::End();
		}

		// Create tabs
		if (ImGui::Begin("Bot", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
		{
			ImGui::SeparatorText("Welcome to the Bot Manager!");

			// Add vertical padding before the table
    		ImGui::Dummy(ImVec2(0.0f, 10.0f));
			if (ImGui::BeginTable("##botTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable))
			{
				float minTasksPanelWidth = std::max(200.0f, ImGui::GetContentRegionAvail().x * 0.2f);
				ImGui::TableSetupColumn("##tasksPanel", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoHeaderLabel, minTasksPanelWidth);
				ImGui::TableSetupColumn("##botView", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoHeaderLabel);

				// ===================================== //
				//    Tasks Panel (Task Configuration)   //
				// ===================================== //
				ImGui::TableNextColumn();
				{
					ImGui::BeginChild("Tasks Panel", {0, 0}, true);
					ImVec2 windowPos = ImGui::GetWindowPos();
					ImGui::Dummy({0, 2}); // Padding for title box

					ImGui::Text("Use this panel to configure the bot's tasks.");
					static int numTasks = 0;
					if (ImGui::Button("Add Task"))
					{
						numTasks++;
					}

					for (int i = 0; i < numTasks; i++)
					{
						ImGui::PushID(i);
						ImGui::BeginChild("Task", ImVec2(0, 30), true, ImGuiWindowFlags_NoScrollbar);
						ImGui::Text("This task is a placeholder.");
						ImGui::EndChild();
						// ImGui::Spacing(); // Add some spacing between tasks
						ImGui::PopID();
					}

					ImGui::EndChild();

					drawWindowTitle("Tasks Panel", windowPos);
				}

				// ===================================== //
				// Bot View Panel (Screen Capture + Bot) //
				// ===================================== //
				ImGui::TableNextColumn();
				{
					float textLineHeight = ImGui::GetTextLineHeight();
					ImGui::BeginChild("Bot Manager", {0, 60}, ImGuiChildFlags_Border);
					ImVec2 windowPos = ImGui::GetWindowPos();
					ImGui::Dummy({0, 2}); // Padding for title box

					ImGui::Text("Use this panel to control the bot.");
					static bool isBotRunning = false;
					if (ImGui::Button(isBotRunning ? "Stop Bot" : "Start Bot"))
					{
						isBotRunning = !isBotRunning;
					}

					if (isBotRunning)
					{
						runBotInference(image, model, detections);
					}

					ImGui::EndChild();
					drawWindowTitle("Bot Manager", windowPos);

					ImGui::BeginChild("Screen View", {0, 0}, false);

					// Get available size
					ImVec2 availableSize = ImGui::GetContentRegionAvail();

					// Calculate aspect ratio
					float aspectRatio = image.cols / (float)image.rows;
					float displayWidth = availableSize.x;
					float displayHeight = availableSize.y;

					if (displayWidth / aspectRatio <= displayHeight)
					{
						displayHeight = displayWidth / aspectRatio;
					} else {
						displayWidth = displayHeight * aspectRatio;
					}

					// Compute vertical padding
					float verticalPadding = (availableSize.y - displayHeight - ImGui::GetStyle().WindowPadding.y) / 2;
					ImGui::Dummy({0, verticalPadding});

					// Upload the image data to the texture
					glBindTexture(GL_TEXTURE_2D, screenCaptureTex);
					glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, image.cols, image.rows, 0, GL_BGR, GL_UNSIGNED_BYTE, image.data);
					ImGui::Image((void*)(intptr_t)screenCaptureTex, ImVec2(displayWidth, displayHeight));

					// Ensures we cover any remaining vertical space
					ImGui::Dummy({0, verticalPadding});

					ImGui::EndChild();
				}
				ImGui::EndTable();
			}
			ImGui::End();
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

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

	DeleteDC(hdc);

    glfwDestroyWindow(window);
    glfwTerminate();

	shutdownCaptureApi();
    return 0;
}