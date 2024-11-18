#include <bot/taskWorkshopWindow.h>

// Std dependencies
#include <vector>

// Third party dependencies
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <fmt/core.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

// Internal dependencies
#include <system/windowCaptureService.h>
#include <system/mouseMovementDatabase.h>
#include <system/inputManager.h>
#include <utils.h>

TaskWorkshopWindow::TaskWorkshopWindow(GLFWwindow* window) : IBotWindow(window), _captureService(WindowCaptureService::GetInstance())
	, _mouseMovementDatabase(MouseMovementDatabase::GetInstance()), _inputManager(InputManager::GetInstance())
{
	// Create a texture for the screen capture
	glGenTextures(1, &_frameTexId);
	glBindTexture(GL_TEXTURE_2D, _frameTexId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

TaskWorkshopWindow::~TaskWorkshopWindow()
{
	glDeleteTextures(1, &_frameTexId);
}

void TaskWorkshopWindow::Run(float deltaTime)
{
	// Fetch mouse-movements from the database
	auto& mouseMovements = _mouseMovementDatabase.GetMovements();

	// Abort signal is processed first
	if (_inputManager.IsEscapePressed())
	{
		_captureMouseMovement = false;
		_curMouseMovement = nullptr;
		_playbackMouseMovement = false;
		// Make sure we release the mouse click if we are stopping playback
		if (_playbackClickState == MOUSE_CLICK_DOWN)
		{
			_inputManager.SetMousePosition(_mousePos, MOUSE_BUTTON_LEFT, MOUSE_CLICK_UP);
		}
		_playbackClickState = MOUSE_CLICK_NONE;
	}

	_inputManager.GetMousePosition(_mousePos);
	// TODO: Track other button states
	bool mouseUp 	= _inputManager.GetMouseUpPosition(_mouseUp, MOUSE_BUTTON_LEFT);
	bool mouseDown	= _inputManager.GetMouseDownPosition(_mouseDown, MOUSE_BUTTON_LEFT);
	if (_captureMouseMovement && (mouseUp || mouseDown)) // Finish last mouse movement and start a new one
	{
		if (_curMouseMovement != nullptr)
		{
			_curMouseMovement->AddPoint(cv::Point(mouseUp ? _mouseUp : _mouseDown), deltaTime);
		}
		mouseMovements.push_back(MouseMovement());
		_curMouseMovement = &mouseMovements.back();
		_curMouseMovement->AddPoint(cv::Point(mouseUp ? _mouseUp : _mouseDown), deltaTime);
		_curMouseMovement->color = generateRandomColor();
	}
	else if (_captureMouseMovement && _curMouseMovement != nullptr) // Capture the mouse movement
	{
		MousePoint* lastPoint = nullptr;
		if (!_curMouseMovement->points.empty())
		{
			lastPoint = &_curMouseMovement->points.back();
		}
		if (lastPoint != nullptr && lastPoint->pos == _mousePos)
		{
			lastPoint->deltaTime += deltaTime;
			lastPoint->deltaTime = std::min(lastPoint->deltaTime, _samePosThreshold); // Cap the delta time to 1 second
		}
		else
		{
			_curMouseMovement->AddPoint(_mousePos, deltaTime);
		}
	}

	// Playback mode
	if (_playbackMouseMovements.empty())
	{
		_playbackMouseMovement = false;

		// Make sure we release the mouse click if we are stopping playback
		if (_playbackClickState == MOUSE_CLICK_DOWN)
		{
			_inputManager.SetMousePosition(_mousePos, MOUSE_BUTTON_LEFT, MOUSE_CLICK_UP);
		}
		_playbackClickState = MOUSE_CLICK_NONE;
	}
	if (_playbackMouseMovement)
	{
		if (_curMouseMovement == nullptr) // Pick first movement
		{
			_curMouseMovement = &_playbackMouseMovements[0];
		}

		// Consume the movement (but only if point time is up)
		MousePoint point = _curMouseMovement->points[0];
		point.deltaTime -= deltaTime;
		if (point.deltaTime <= 0.0f) // Consume point
		{
			_curMouseMovement->points.erase(_curMouseMovement->points.begin());
			if (_curMouseMovement->points.empty()) // Last point consumed
			{
				if (_playbackClickState == MOUSE_CLICK_NONE) _playbackClickState = MOUSE_CLICK_DOWN;
				else _playbackClickState = (MouseClickState)(1 - _playbackClickState); // Flip state
				_inputManager.SetMousePosition(point.pos, MOUSE_BUTTON_LEFT, _playbackClickState);

				_playbackMouseMovements.erase(_playbackMouseMovements.begin());
				_curMouseMovement = nullptr;
			}
			else
			{
				_inputManager.SetMousePosition(point.pos);
			}
		}
	}

	if (ImGui::Begin("Tasks", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
	{
		// Fetch a new copy of the image
		_frame = _captureService.GetLatestFrame();

		ImGui::SeparatorText("This is the Task Workshop! Use it to create and test new tasks.");
		if (ImGui::BeginTable("##taskWorkshopTable", 3, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable))
		{
			float minTasksPanelWidth = std::max(200.0f, ImGui::GetContentRegionAvail().x * 0.2f);
			ImGui::TableSetupColumn("##taskListPanel", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoHeaderLabel, minTasksPanelWidth);
			ImGui::TableSetupColumn("##frameViewPanel", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoHeaderLabel);
			ImGui::TableSetupColumn("##taskStepsPanel", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoHeaderLabel);

			// ======================== //
			//     Statistics Panel     //
			// ======================== //
			ImGui::TableNextColumn();
			{
				ImGuiPanelGuard statsPanel("Statistics Panel");

				ImGui::SeparatorText("Mouse Movement");
				ImGui::Text("Mouse position: (%i, %i)", _mousePos.x, _mousePos.y);
				ImGui::Text("Mouse down position: (%i, %i)", _mouseDown.x, _mouseDown.y);
				ImGui::Text("Mouse release position: (%i, %i)", _mouseUp.x, _mouseUp.y);

				ImGui::Text("Mouse Initial-End Distance: %.2f", _curMouseMovement == nullptr ? 0.0f : _curMouseMovement->IniEndDistance());

				ImGui::Separator();
				ImGui::SeparatorText("Mouse Movement Capture");
				if (ImGui::Button(_captureMouseMovement ? "Stop capturing" : "Start capturing"))
				{
					_captureMouseMovement = !_captureMouseMovement;
					if (_captureMouseMovement)
					{
						_playbackMouseMovement = false;
						// Make sure we release the mouse click if we are stopping playback
						if (_playbackClickState == MOUSE_CLICK_DOWN)
						{
							_inputManager.SetMousePosition(_mousePos, MOUSE_BUTTON_LEFT, MOUSE_CLICK_UP);
						}
						_playbackClickState = MOUSE_CLICK_NONE;
					}
				}
				// Remove first and last movements when button clicked
				if (ImGui::IsItemHovered() && (mouseDown || mouseUp) && _curMouseMovement != nullptr)
				{
					mouseMovements.pop_back(); // Right before click (we process clicks before ui)
					if (mouseDown) mouseMovements.pop_back(); // Movement to click position
					_curMouseMovement = nullptr;
				}
				ImGui::SameLine();
				if (ImGui::Button(_playbackMouseMovement ? "Stop playback" : "Start playback"))
				{
					_playbackMouseMovement = !_playbackMouseMovement;
					if (_captureMouseMovement)
					{
						_captureMouseMovement = false;
					}

					// Copy the movements to the playback movements
					if (_playbackMouseMovement)
					{
						_playbackMouseMovements = mouseMovements;
					}
				}
				// Remove first and last movements when button clicked
				if (ImGui::IsItemHovered() && mouseDown && _curMouseMovement != nullptr)
				{
					mouseMovements.pop_back(); // Right before click (we process clicks before ui)
					mouseMovements.pop_back(); // Movement to click position
					_curMouseMovement = nullptr;
				}

				ImGui::TextUnformatted("Same Position Threshold");
				ImGui::SameLine();
				ImGui::DragFloat("##_samePosThreshold", &_samePosThreshold, 0.5f, 0.5, 60.0, "%.3f seconds");
				if (_curMouseMovement != nullptr && _captureMouseMovement)
				{
					ImGui::TextUnformatted("Current Mouse Movement:");
					size_t numPoints = _curMouseMovement->points.size();
					float iniEndDist = _curMouseMovement->IniEndDistance();
					float curPointLifetime = 0.0f;
					if (numPoints > 0) curPointLifetime = _curMouseMovement->points.back().deltaTime;
					ImGui::Text("[%zu points] [%.2f dist] [%.4f point lifetime]", numPoints, iniEndDist, curPointLifetime);
				}

				ImGui::Separator();
				ImGui::Checkbox("Draw All Mouse Movements", &_drawMouseMovements);

				ImGui::SameLine();
				if (ImGui::Button("Delete All Movements"))
				{
					mouseMovements.clear();
					_playbackMouseMovements.clear();
					_curMouseMovement = nullptr;
				}

				const bool recordingOrPlaying = (_playbackMouseMovement || _captureMouseMovement);
				ImGui::BeginDisabled(recordingOrPlaying);
				if (ImGui::Button("Save Movements"))
				{
					_mouseMovementDatabase.SaveMovements();
				}
				ImGui::SameLine();
				if (ImGui::Button("Load Movements"))
				{
					_mouseMovementDatabase.LoadMovements();
				}
				ImGui::EndDisabled();

				ImGui::TextUnformatted("Mouse Movements:");
				ImGui::BeginChild("Mouse Movements", {0, 0}, true);
				for (size_t i = 0; i < mouseMovements.size(); i++)
				{
					const auto& movement = mouseMovements[i];
					bool selected = _selMouseMovement == &movement;
					float movementDuration = 0.0f;
					for (const auto& point : movement.points)
					{
						movementDuration += point.deltaTime;
					}
					std::string label = fmt::format("Movement {} [{} points] [{:.2f} dist] [{:.2f}s]",
								i, movement.points.size(), movement.IniEndDistance(), movementDuration);
					if (ImGui::Selectable(label.c_str(), selected))
					{
						if (!recordingOrPlaying) _selMouseMovement = &mouseMovements[i];
					}
					if (ImGui::IsItemHovered())
					{
						ImGui::BeginTooltip();
						ImGui::Text("Initial Point: (%i, %i)", movement.points[0].pos.x, movement.points[0].pos.y);
						ImGui::Text("End Point: (%i, %i)", movement.points.back().pos.x, movement.points.back().pos.y);
						ImGui::TextUnformatted("Click to select, then 'Delete' to remove this movement.");
						ImGui::EndTooltip();
						_hovMouseMovement = &mouseMovements[i];
					}
				}
				if (_selMouseMovement != nullptr && glfwGetKey(_nativeWindow, GLFW_KEY_DELETE) == GLFW_PRESS)
				{
					size_t rmvId = _selMouseMovement - mouseMovements.data();
					if (_hovMouseMovement == _selMouseMovement) _hovMouseMovement = nullptr;
					_selMouseMovement = nullptr;
					mouseMovements.erase(mouseMovements.begin() + rmvId);
				}
				if (mouseMovements.empty())
				{
					ImGui::Text("No captured mouse-movement at the moment.");
				}
				ImGui::EndChild();
			}

			ImGui::TableNextColumn();
			{
				ImGuiPanelGuard screenView("Screen View");
				drawScreenView(_frame, _frameTexId);
			}

			ImGui::TableNextColumn();
			{
				ImGuiPanelGuard analysisPanel("Analysis Panel");
				ImGui::Text("Movement Distances:");
			}

			ImGui::EndTable();
		}

		ImGui::End();
	}
}