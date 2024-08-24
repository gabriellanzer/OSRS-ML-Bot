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
#include <system/mouseMovementDB.h>
#include <system/mouseTracker.h>
#include <utils.h>

TrainingLabWindow::TrainingLabWindow(GLFWwindow* window) : IBotWindow(window), _captureService(WindowCaptureService::getInstance())
	, _mouseMovementDatabase(MouseMovementDatabase::GetInstance())
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

static void drawMouseMovement(const MouseMovement& movement, cv::Mat& frame, int thickness = 2, cv::Scalar colorOverride = cv::Scalar(-1, -1, -1))
{
	// Fetch capture min and max points
	auto [min, max] = WindowCaptureService::getInstance().GetCaptureDimensions();

	cv::Scalar color = movement.color;
	if (colorOverride != cv::Scalar(-1, -1, -1))
	{
		color = colorOverride;
	}

	// Draw markers for first and last points
	if (!movement.points.empty())
	{
		cv::Point p1 = movement.points[0].point;
		cv::Point p2 = movement.points.back().point;
		// Clamp to windows coordinates
		p1.x = std::clamp(p1.x, min.x, max.x - 1);
		p1.y = std::clamp(p1.y, min.y, max.y - 1);
		p2.x = std::clamp(p2.x, min.x, max.x - 1);
		p2.y = std::clamp(p2.y, min.y, max.y - 1);
		// Wrap around if negative
		if (p1.x < 0) p1.x += frame.cols;
		if (p1.y < 0) p1.y += frame.rows;
		if (p2.x < 0) p2.x += frame.cols;
		if (p2.y < 0) p2.y += frame.rows;
		cv::drawMarker(frame, p1, color, cv::MARKER_CROSS, 80, thickness);
		cv::drawMarker(frame, p2, color, cv::MARKER_CROSS, 80, thickness);
	}
	for (size_t i = 1; i < movement.points.size(); i++)
	{
		cv::Point p1 = movement.points[i - 1].point;
		cv::Point p2 = movement.points[i].point;
		// Clamp to windows coordinates
		p1.x = std::clamp(p1.x, min.x, max.x - 1);
		p1.y = std::clamp(p1.y, min.y, max.y - 1);
		p2.x = std::clamp(p2.x, min.x, max.x - 1);
		p2.y = std::clamp(p2.y, min.y, max.y - 1);
		// Wrap around if negative
		if (p1.x < 0) p1.x += frame.cols;
		if (p1.y < 0) p1.y += frame.rows;
		if (p2.x < 0) p2.x += frame.cols;
		if (p2.y < 0) p2.y += frame.rows;
		if (p1 == p2)
		{
			cv::circle(frame, p1, 0, color, thickness);
		}
		else
		{
			cv::line(frame, p1, p2, color, thickness);
		}
	}
}

void TrainingLabWindow::Run(float deltaTime)
{
	// Fetch mouse-movements from the database
	auto& mouseMovements = _mouseMovementDatabase.GetMovements();

	// Abort signal is processed first
	if (_mouseTracker->IsEscapePressed())
	{
		_captureMouseMovement = false;
		_playbackMouseMovement = false;
		_curMouseMovement = nullptr;
	}

	_mouseTracker->GetMousePosition(_mousePos);
	bool mouseUp 	= _mouseTracker->GetMouseUpPosition(_mouseUp);
	bool mouseDown	= _mouseTracker->GetMouseDownPosition(_mouseDown);
	if (_captureMouseMovement && mouseUp) // Finish last mouse-up movement and start a new one
	{
		if (_curMouseMovement != nullptr)
		{
			_curMouseMovement->AddPoint(cv::Point(_mouseDown), deltaTime);
			_curMouseMovement->clickState = MOUSE_UP;
		}
		mouseMovements.push_back(MouseMovement());
		_curMouseMovement = &mouseMovements.back();
		_curMouseMovement->AddPoint(cv::Point(_mouseUp), deltaTime);
		_curMouseMovement->color = generateRandomColor();
	}
	else if (_captureMouseMovement && mouseDown) // Finish last mouse-down movement and start a new one
	{
		if (_curMouseMovement != nullptr)
		{
			_curMouseMovement->AddPoint(cv::Point(_mouseDown), deltaTime);
			_curMouseMovement->clickState = MOUSE_DOWN;
		}
		mouseMovements.push_back(MouseMovement());
		_curMouseMovement = &mouseMovements.back();
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
	}
	if (_playbackMouseMovement)
	{
		_internalTimer += deltaTime;
		if (_curMouseMovement == nullptr) // Pick first movement
		{
			_curMouseMovement = &_playbackMouseMovements[0];
		}

		// Consume the movement (but only if point time is up)
		const MousePoint point = _curMouseMovement->points[0];
		if (_internalTimer >= point.deltaTime)
		{
			_curMouseMovement->points.erase(_curMouseMovement->points.begin());

			// TODO: take in account release
			_internalTimer -= point.deltaTime;
			if (_curMouseMovement->points.empty()) // Last point consumed
			{
				_mouseTracker->SetMousePosition(point.point, _curMouseMovement->clickState);
				_playbackMouseMovements.erase(_playbackMouseMovements.begin());
				_curMouseMovement = nullptr;
			}
			else
			{
				_mouseTracker->SetMousePosition(point.point);
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
					const char* movementType = movement.clickState == MOUSE_DOWN ? "Mouse Down" : "Mouse Up";
					std::string label = fmt::format("Movement {} [{} points] [{:.2f} dist] [{} type] [{:.2f}s]",
								i, movement.points.size(), movement.IniEndDistance(), movementType, movementDuration);
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
					static float screenViewHeight = max(300.0f, totalHeight * 0.6f); // Initial height
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
						drawMouseMovement(*_selMouseMovement, _frame, 3);
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
						ImGuiPanelGuard analysisPanel("Analysis Panel", { 0, analysisPanelHeight });

						// TODO: optimize this
						// Get all distances
						float minDist = FLT_MAX, maxDist = 0.0f, avgDist = 0.0f;
						std::vector<float> distances;
						for (const auto& movement : mouseMovements)
						{
							float distance = movement.IniEndDistance();
							minDist = std::min(minDist, distance);
							maxDist = std::max(maxDist, distance);
							avgDist += distance;
							distances.push_back(distance);
						}
						avgDist /= mouseMovements.size();

						// Compute histogram data
						std::sort(distances.begin(), distances.end());

						std::map<float, int> histogram;
						float minHeight = FLT_MAX, maxHeight = 0;
						const float bin_width = 50;
						for (const auto& dist : distances)
						{
							float bin = std::floor(dist / bin_width) * bin_width;
							++histogram[bin];
							minHeight = std::min(minHeight, (float)histogram[bin]);
							maxHeight = std::max(maxHeight, (float)histogram[bin]);
						}
						minHeight -= 1; // Show 1-sized elements
						maxHeight += 4; // Add some vertical padding
						std::vector<float> histogramDistData;
						std::vector<float> histogramCountData;
						histogramDistData.reserve(histogram.size());
						histogramCountData.reserve(histogram.size());
						for (const auto& [dist, count] : histogram)
						{
							// Find the insert position based on distance
							auto it = std::lower_bound(histogramDistData.begin(), histogramDistData.end(), dist);
							auto insertDist = std::distance(histogramDistData.begin(), it);
							histogramDistData.insert(it, dist);
							histogramCountData.insert(histogramCountData.begin() + insertDist, count);
						}

						auto [availableWidth, availableHeight] = ImGui::GetContentRegionAvail();
						float plotWidth = std::min(availableWidth, 600.0f);
						ImGui::Text("Movement Distances:");
						if (mouseMovements.empty())
						{
							ImGui::Text("No movements captured.");
						}
						else
						{
							ImGui::Text("Min [%.2f] Max [%.2f] Avg [%.2f]", minDist, maxDist, avgDist);
							ImGui::Dummy({ (availableWidth - plotWidth) / 2.0f, 0 }); ImGui::SameLine();
							ImGui::PlotHistogram("##histogram", histogramCountData.data(), histogramCountData.size(),
								0, "Number of paths by distance (granularity = 50px)", minHeight, maxHeight, ImVec2(plotWidth, availableHeight * 0.9f));
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