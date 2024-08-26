#include <bot/botManagerWindow.h>

// Std dependencies
#include <string>
#include <iostream>

// Third party dependencies
#include <nfd.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <glad/glad.h>

// Internal dependencies
#include <system/windowCaptureService.h>
#include <system/mouseMovementDatabase.h>
#include <system/inputManager.h>
#include <ml/onnxruntimeInference.h>
#include <utils.h>


BotManagerWindow::BotManagerWindow(GLFWwindow* window) : IBotWindow(window)
	, _inputManager(InputManager::GetInstance())
	, _captureService(WindowCaptureService::GetInstance())
	, _mouseMovementDatabase(MouseMovementDatabase::GetInstance())
{
	// Create a texture for the screen capture
	glGenTextures(1, &_frameTexId);
	glBindTexture(GL_TEXTURE_2D, _frameTexId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	_modelPath = nullptr;
}

BotManagerWindow::~BotManagerWindow()
{
	if (_modelPath != nullptr) delete[] _modelPath;
	if (_model != nullptr) delete _model;
	glDeleteTextures(1, &_frameTexId);
}

void BotManagerWindow::Run(float deltaTime)
{
	// Fetch a new copy of the image
	_frame = _captureService.GetLatestFrame();

	if (_inputManager.IsEscapePressed())
	{
		_isBotRunning = false;
		_curTargetBoxState = nullptr;
	}

	if (_isBotRunning)
	{
		runBotInference(deltaTime, _frame);
	}

	if (ImGui::Begin("Bot", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
	{
		ImGui::SeparatorText("Welcome to the Bot Manager!");

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
				ImGuiPanelGuard taskPanel("Tasks Panel");

				static int numTasks = 0;
				ImGui::TextWrapped("Use this panel to configure the bot's tasks.");

				ImGui::TextUnformatted("Hardcoded Task:");
				ImGui::TextUnformatted("Mine Copper Ore");

				// Print current movement information
				if (_curMouseMovement.IsValid())
				{
					ImGui::Text("Current Movement:");
					ImGui::Text("Points: %zu", _curMouseMovement.points.size());
					ImGui::Text("Distance: %.2f", _curMouseMovement.IniEndDistance());
					ImGui::Text("Angle: %.2f", _curMouseMovement.GetAngle());

					// Current point information
					if (!_curMouseMovement.points.empty())
					{
						const MousePoint& curPoint = _curMouseMovement.points[0];
						ImGui::Text("Current Point:");
						ImGui::Text("X: %d, Y: %d", curPoint.pos.x, curPoint.pos.y);
						ImGui::Text("Delta Time: %.2f", curPoint.deltaTime);
					}
				}

				ImGui::Separator();
				if (ImGui::Button("Add Task"))
				{
					numTasks++;
				}

				for (int i = 0; i < numTasks; i++)
				{
					ImGui::PushID(i);
					ImGui::BeginChild("Task", ImVec2(0, 50), true, ImGuiWindowFlags_NoScrollbar);
					ImGui::Text("This task is a placeholder.");
					if (ImGui::Button("Delete Task"))
					{
						numTasks--;
					}
					ImGui::EndChild();
					// ImGui::Spacing(); // Add some spacing between tasks
					ImGui::PopID();
				}
			}

			// ===================================== //
			// Bot View Panel (Screen Capture + Bot) //
			// ===================================== //
			ImGui::TableNextColumn();
			{
				{
					ImGuiPanelGuard botManager("Bot Manager", { 0, 60 });

					ImGui::Text("Use this panel to control the bot.");
					if (!_isBotRunning)
					{
						const bool isModelUnloaded = _model == nullptr;
						ImGui::BeginDisabled(isModelUnloaded);
						if (ImGui::Button("Start Bot"))
						{
							_isBotRunning = true;
							_mouseMovementDatabase.LoadMovements();
						}
						ImGui::EndDisabled();
						if (isModelUnloaded && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
						{
							ImGui::BeginTooltip();
							ImGui::Text("Load a model before starting the bot.");
							ImGui::EndTooltip();
						}

						ImGui::SameLine();
						ImGui::BeginDisabled(_modelPath == nullptr);
						if (ImGui::Button(isModelUnloaded ? "Load Model" : "Reload Model"))
						{
							// Cleanup old model
							if (_model != nullptr)
							{
								delete _model;
							}

							// Load new model
							_model = new YOLOv8(8 /*8 ore categories (subjected to change)*/, 0.85);
							_model->LoadModel(true, _modelPath);

							// And run warm-up inference
							_boxStates.clear();
							runBotInference(deltaTime, _frame);
						}
						ImGui::EndDisabled();
						if (_modelPath == nullptr && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
						{
							ImGui::BeginTooltip();
							ImGui::Text("Select a model file first.");
							ImGui::EndTooltip();
						}

						ImGui::SameLine();
						static std::string modelPathStr;
						if (_modelPath != nullptr)
						{
							modelPathStr = WideStringToUTF8(_modelPath);
						}
						else
						{
							modelPathStr.clear();
						}

						// Set darker background color for the input text
						ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
						ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
						ImGui::InputTextWithHint("##modelPath", "Click to select model path...", modelPathStr.data(), modelPathStr.size(), ImGuiInputTextFlags_ReadOnly);
						ImGui::PopStyleColor();

						if (ImGui::IsItemClicked())
						{
							NFD::Guard dialogGuard;
							nfdnchar_t* outPath = nullptr;
							nfdresult_t result = NFD::OpenDialog(outPath);
							if (result == NFD_OKAY)
							{
								if (_modelPath != nullptr) delete[] _modelPath;
								size_t pathLen = wcslen(outPath) + 1;
								_modelPath = new wchar_t[pathLen];
								std::memcpy(_modelPath, outPath, pathLen * sizeof(wchar_t));

								NFD::FreePath(outPath);
							}
							else if (result == NFD_ERROR)
							{
								const char* error = NFD::GetError();
								std::cerr << "Error: " << error << std::endl;
							}
						}
					}
					else
					{
						if (ImGui::Button("Stop Bot"))
						{
							_isBotRunning = false;
						}
					}
				}

				{
					ImGuiPanelGuard botManager("Screen View", { 0, 0 });
					drawScreenView(_frame, _frameTexId);
				}
			}
			ImGui::EndTable();
		}
		ImGui::End();
	}
}

void BotManagerWindow::runBotInference(float deltaTime, cv::Mat& frame)
{
	// Run YOLO inference
	_model->Inference(frame, _detections);

	// Filter out the detections that overlap
	size_t detectionCount = _detections.size();
	for (int i = 0; i < detectionCount; ++i)
	{
		YoloDetectionBox& curBox = _detections[i];
		for (int j = i + 1; j < _detections.size(); j++)
		{
			YoloDetectionBox& otherBox = _detections[j];
			if (curBox.IsSimilar(otherBox))
			{
				// Skip if class missmatch
				if (curBox.classId != otherBox.classId) continue;

				// Merge the two detections in current
				curBox = curBox.Merge(otherBox);

				// Swap with last and pop
				_detections[j] = _detections.back();
				_detections.pop_back();

				// Prevent j increment to check the new box
				--j;
			}
		}
	}

	// Before we update the box states, we need to increment the last seen
	// time for each box state, and remove the ones that have not been seen
	for (int i = _boxStates.size() - 1; i >= 0; --i)
	{
		auto& boxState = _boxStates[i];
		boxState.lastSeen += deltaTime;
		if (boxState.lastSeen > 10.0f) // TODO: Make this a configurable value
		{
			// Reset clicked pointer
			if (_curTargetBoxState == &boxState)
			{
				_curTargetBoxState = nullptr;
			}
			_boxStates.erase(_boxStates.begin() + i);
		}
	}

	// We keep track of the mapping between detections and box states so we can make the bot operate on
	// detections (most valid screen data) while also being able to peel off the states for processing
	// Info.: All detections have a box state, but not all box states have a valid detection
	_boxStatesMapping.resize(_detections.size());

	// Try to map current detections to persisted box states
	// If a detection is not found, add it to the box states
	for (int i = 0; i < _detections.size(); ++i)
	{
		bool found = false;
		const auto& detection = _detections[i];
		for (auto& boxState : _boxStates)
		{
			if (boxState.box.IsSimilar(detection))
			{
				// Doing this so bot knows object has changed
				// TODO: This is something a task should handle
				if (boxState.box.classId != detection.classId)
				{
					// Reset clicked pointer
					if (_curTargetBoxState == &boxState)
					{
						_curTargetBoxState = nullptr;
					}
				}

				boxState.box = detection;
				boxState.lastSeen = 0.0f;
				found = true;

				_boxStatesMapping[i] = &boxState;
				break;
			}
		}

		if (!found)
		{
			_boxStates.push_back({ detection, 0.0f });
			_boxStatesMapping[i] = &_boxStates.back();
		}
	}

	// TODO: Draw using ImGui or OpenGL (for performance),
	// and move this to a proper wrapper class per model
	// Draw rectangles on the image frame
	for (int i = 0; i < _boxStates.size(); ++i)
	{
		const auto& detection = _boxStates[i].box;
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
		cv::rectangle(frame, rect, color, 2);
		label += " (" + std::to_string(i) + ")";
		cv::putText(frame, label, rect.tl() - cv::Point{0, 15}, cv::HersheyFonts::FONT_HERSHEY_PLAIN, 1.0, color, 2);
	}

	// Update mouse movement database
	_mouseMovementDatabase.UpdateDatabase();

	// TEST: Fetch closes copper ore to center of screen (player) and
	// query a mouse movement from current mouse position to that point
	cv::Point playerPos(frame.cols / 2, frame.rows / 2);
	cv::Point closestCopperPos;
	float closestCopperRadius = 0.0f;
	float closestCopperDistance = FLT_MAX;
	const YoloDetectionBox* closestCopperDetection = nullptr;
	for (const auto& detection : _detections)
	{
		if (detection.classId != 2) continue; // Copper

		const float halfWidth = detection.w / 2;
		const float halfHeight = detection.h / 2;
		cv::Point copperPos(detection.x + halfWidth, detection.y + halfHeight);
		float distance = cv::norm(playerPos - copperPos);
		if (distance < closestCopperDistance)
		{
			closestCopperDistance = distance;
			closestCopperPos = copperPos;
			closestCopperRadius = std::min(halfWidth, halfHeight);
			closestCopperDetection = &detection;
		}
	}

	// If no copper ore was found (and no current target box active), return
	if (closestCopperDetection == nullptr && _curTargetBoxState == nullptr)
	{
		return;
	}

	// Fetch mouse position for query
	cv::Point mousePos;
	_inputManager.GetMousePosition(mousePos);

	// Convert target pos from image frame coordinates to system coordinates
	closestCopperPos = _captureService.FrameToSystemCoordinates(closestCopperPos, frame);

	// Query mouse movement from current mouse
	MouseMovement bestMovement;
	_mouseMovementDatabase.QueryMovement(mousePos, closestCopperPos, closestCopperRadius, bestMovement);

	if (bestMovement.IsValid())
	{
		drawMouseMovement(bestMovement, frame);
	}

	// If we don't we are moving to the next ore (current movement)
	if (_curMouseMovement.IsValid())
	{
		// Play the movement
		MousePoint& point = _curMouseMovement.points[0];
		point.deltaTime -= deltaTime;
		if (point.deltaTime <= 0.0f) // Consume point
		{
			cv::Point mousePos = point.pos;
			_curMouseMovement.points.erase(_curMouseMovement.points.begin());
			if (_curMouseMovement.points.empty()) // Last point consumed (click)
			{
				if (_curClickState == MOUSE_CLICK_NONE) _curClickState = MOUSE_CLICK_DOWN;
				else _curClickState = (MouseClickState)(1 - _curClickState); // Flip state
				_inputManager.SetMousePosition(mousePos, MOUSE_BUTTON_LEFT, _curClickState);

				// If we just clicked down, fetch a click-up and set it as the next movement
				if (_curClickState == MOUSE_CLICK_DOWN)
				{
					// Where we release the click doesn't really matter for mining (could for other tasks)
					_mouseMovementDatabase.QueryMovement(mousePos, mousePos, 200.0f, _curMouseMovement);
				}
				else // Click up (mouse move + click finished)
				{
					// Assign current target box state as the one we just started mining
					int detectionIndex = closestCopperDetection - _detections.data();
					_curTargetBoxState = _boxStatesMapping[detectionIndex];
				}
			}
			else // Not final point yet, just move the mouse
			{
				_inputManager.SetMousePosition(mousePos);
			}
		}
	}
	else // We are waiting for the ore to be mined or we are idle
	{
		// If we have a target box, we are waiting for the ore to be mined
		if (_curTargetBoxState != nullptr)
		{
			// TODO: Do something here (like check if the ore is mined?)
			bool test = true;
		}
		else // We are idle
		{
			// If we have a valid movement, we can start moving to the next ore
			if (bestMovement.IsValid())
			{
				_curMouseMovement = std::move(bestMovement);
			}
		}
	}
}