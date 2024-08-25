#include <bot/trainingLabWindow.h>

// Std dependencies
#include <map>
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

void ComputeMovementsHistogram(const std::vector<MouseMovement>& mouseMovements, std::vector<float>& histogram, const int binWidth = 50)
{
	// Get all distances
	std::vector<float> distances;
	distances.reserve(mouseMovements.size());
	for (const auto& movement : mouseMovements)
	{
		distances.push_back(movement.IniEndDistance());
	}

	// Compute histogram data
	std::sort(distances.begin(), distances.end());

	// Clear now so we can early-out if there are no matches
	histogram.clear();
	if (distances.empty())
	{
		return;
	}

	// Compute number of bins from min and max distances
	float minDistance = distances[0];
	float maxDistance = distances.back();
	int minBin = minDistance / binWidth;
	int maxBin = maxDistance / binWidth;
	int numBins = maxBin - minBin + 1;
	histogram.resize(numBins);
	for (const auto& dist : distances)
	{
		int bin = dist / binWidth - minBin;
		histogram[bin] += 1;
	}
}

TrainingLabWindow::TrainingLabWindow(GLFWwindow* window) : IBotWindow(window), _captureService(WindowCaptureService::GetInstance())
	, _mouseMovementDatabase(MouseMovementDatabase::GetInstance()), _inputManager(InputManager::GetInstance())
{
	// Create a texture for the screen capture
	glGenTextures(1, &_frameTexId);
	glBindTexture(GL_TEXTURE_2D, _frameTexId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

TrainingLabWindow::~TrainingLabWindow()
{
	glDeleteTextures(1, &_frameTexId);
}

void TrainingLabWindow::Run(float deltaTime)
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
		if (lastPoint != nullptr && lastPoint->point == _mousePos)
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
				_inputManager.SetMousePosition(point.point, MOUSE_BUTTON_LEFT, _playbackClickState);

				_playbackMouseMovements.erase(_playbackMouseMovements.begin());
				_curMouseMovement = nullptr;
			}
			else
			{
				_inputManager.SetMousePosition(point.point);
			}
		}
	}

	if (ImGui::Begin("Training", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
	{
		// Fetch a new copy of the image
		_frame = _captureService.GetLatestFrame();
		// TODO: Draw the mouse-movement lines on the image

		ImGui::SeparatorText("Welcome to the Training Lab!");

		if (ImGui::BeginTable("##trainingTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable))
		{
			float minTasksPanelWidth = std::max(200.0f, ImGui::GetContentRegionAvail().x * 0.2f);
			ImGui::TableSetupColumn("##statisticsPanel", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoHeaderLabel, minTasksPanelWidth);
			ImGui::TableSetupColumn("##analysisPanel", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoHeaderLabel);

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
						ImGui::Text("Initial Point: (%i, %i)", movement.points[0].point.x, movement.points[0].point.y);
						ImGui::Text("End Point: (%i, %i)", movement.points.back().point.x, movement.points.back().point.y);
						ImGui::TextUnformatted("Click to select, then 'Delete' to remove this movement.");
						ImGui::EndTooltip();
					}
				}
				if (_selMouseMovement != nullptr && glfwGetKey(_nativeWindow, GLFW_KEY_DELETE) == GLFW_PRESS)
				{
					size_t rmvId = _selMouseMovement - mouseMovements.data();
					mouseMovements.erase(mouseMovements.begin() + rmvId);
					_selMouseMovement = nullptr;
				}
				if (mouseMovements.empty())
				{
					ImGui::Text("No captured mouse-movement at the moment.");
				}
				ImGui::EndChild();
			}

			ImGui::TableNextColumn();
			{
				if (ImGui::BeginTable("##screenViewAndAnalysis", 1, ImGuiTableFlags_Resizable))
				{
					const float totalHeight = ImGui::GetContentRegionAvail().y;
					const float minHeight = totalHeight * 0.2f;
					static float screenViewHeight = std::max(300.0f, totalHeight * 0.6f); // Initial height
					static float analysisPanelHeight; // Initial height
					ImGui::TableSetupColumn("##screenView", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoHeaderLabel, 0.0f, 0.0f);

					// Draw mouse position on the frame
					if (_drawMouseMovements)
					{
						for (const auto& movement : mouseMovements)
						{
							drawMouseMovement(movement, _frame);
						}
					}
					if (_selMouseMovement != nullptr)
					{
						// Draw movement outline first
						drawMouseMovement(*_selMouseMovement, _frame, 4, cv::Scalar(255, 255, 255));
						// Then draw the movement
						drawMouseMovement(*_selMouseMovement, _frame);
					}

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
						ImGuiPanelGuard analysisPanel("Analysis Panel", { 0, analysisPanelHeight - 10 });

						ImGui::Text("Movement Distances:");
						if (mouseMovements.empty())
						{
							ImGui::Text("No movements captured.");
						}
						else
						{
							// Compute histograms
							std::vector<float> mouseUpHistogram;
							std::vector<float> mouseDownHistogram;
							ComputeMovementsHistogram(mouseMovements, mouseUpHistogram);

							auto [availableWidth, availableHeight] = ImGui::GetContentRegionAvail();
							float plotWidth = std::max(availableWidth * 0.7f, 400.0f);

							float paddingWidth = (availableWidth - plotWidth) / 2.0f;
							ImGui::Dummy({ paddingWidth, 0 }); ImGui::SameLine();
							ImGui::PlotHistogram("##histogram", mouseDownHistogram.data(), mouseDownHistogram.size(),
											0, "Number of paths by distance (type = MOUSE_DOWN, granularity = 50px)",
											0.0f, FLT_MAX, ImVec2(plotWidth, availableHeight * 0.9f));
						}
					}

					ImGui::EndTable();
				}
			}

			ImGui::EndTable();
		}

		ImGui::End();
	}
}