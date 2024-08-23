#include <bot/trainingLabWindow.h>

// Std dependencies
#include <string>
#include <vector>

// Third party dependencies
#include <opencv2/core.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <glad/glad.h>

// Internal dependencies
#include <system/windowCaptureService.h>
#include <system/mouseTracker.h>
#include <utils.h>

TrainingLabWindow::TrainingLabWindow() : _captureService(WindowCaptureService::getInstance())
{
	_mouseTracker = new MouseTracker();

	// Create a texture for the screen capture
	glGenTextures(1, &_frameTexId);
	glBindTexture(GL_TEXTURE_2D, _frameTexId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

TrainingLabWindow::~TrainingLabWindow()
{
	glDeleteTextures(1, &_frameTexId);
	delete _mouseTracker;
}

void TrainingLabWindow::Run()
{
	_mouseTracker->GetMousePosition(_mousePosX, _mousePosY);
	bool mouseDown	= _mouseTracker->GetMouseDownPosition(_mouseDownX, _mouseDownY);
	bool mouseUp 	= _mouseTracker->GetMouseUpPosition(_mouseUpX, _mouseUpY);
	if (_captureMouseMovement && mouseDown) // Start a new mouse movement
	{
		_mouseMovements.push_back(MouseMovement());
		_mouseMovements.back().points.push_back(cv::Point(_mouseDownX, _mouseDownY));
	}
	else if (_captureMouseMovement && mouseUp) // Finish the current mouse movement
	{
		_mouseMovements.back().points.push_back(cv::Point(_mouseUpX, _mouseUpY));
	}
	else if (_captureMouseMovement) // Capture the mouse movement
	{
		_mouseMovements.back().points.push_back(cv::Point(_mousePosX, _mousePosY));
	}

	if (ImGui::Begin("Training", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
	{
		// Fetch a new copy of the image
		_frame = _captureService.GetLatestFrame();
		// TODO: Draw the mouse-movement lines on the image

		ImGui::SeparatorText("Welcome to the Training Lab!");

		if (ImGui::BeginTable("##trainingTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable))
		{
			float minTasksPanelWidth = max(200.0f, ImGui::GetContentRegionAvail().x * 0.2f);
			ImGui::TableSetupColumn("##statisticsPanel", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoHeaderLabel, minTasksPanelWidth);
			ImGui::TableSetupColumn("##analysisPanel", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoHeaderLabel);

			// ======================== //
			//     Statistics Panel     //
			// ======================== //
			ImGui::TableNextColumn();
			{
				ImGuiPanelGuard statsPanel("Statistics Panel");

				ImGui::SeparatorText("Mouse Movement");
				ImGui::Text("Mouse position: (%i, %i)", _mousePosX, _mousePosY);
				ImGui::Text("Mouse down position: (%i, %i)", _mouseDownX, _mouseDownY);
				ImGui::Text("Mouse release position: (%i, %i)", _mouseUpX, _mouseUpY);

				ImGui::Separator();
				ImGui::TextUnformatted("Mouse Movement:");
				ImGui::SameLine();
				if (ImGui::Button(_captureMouseMovement ? "Stop capturing" : "Start capturing"))
				{
					_captureMouseMovement = !_captureMouseMovement;
				}
			}

			ImGui::TableNextColumn();
			{
				if (ImGui::BeginTable("##screenViewAndAnalysis", 1, ImGuiTableFlags_Resizable))
				{
					const float totalHeight = ImGui::GetContentRegionAvail().y;
					const float minHeight = totalHeight * 0.2f;
					static float screenViewHeight = max(300.0f, totalHeight * 0.6f); // Initial height
					static float analysisPanelHeight; // Initial height
					ImGui::TableSetupColumn("##screenView", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoHeaderLabel, 0.0f, 0.0f);

					// Screen View
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					{
						ImGuiPanelGuard screenView("Screen View", { 0, screenViewHeight });
						drawScreenView(_frame, _frameTexId);
					}

					// Draggable separator
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					{
						analysisPanelHeight = ImGui::GetContentRegionAvail().y;
						drawHorizontalSeparator(screenViewHeight, analysisPanelHeight, totalHeight, minHeight, minHeight);
					}

					// Analysis Panel
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					{
						ImGuiPanelGuard analysisPanel("Analysis Panel", { 0, analysisPanelHeight });
						ImGui::Text("UNDER CONSTRUCTION");
					}

					ImGui::EndTable();
				}
			}

			ImGui::EndTable();
		}

		ImGui::End();
	}
}