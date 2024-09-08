#include <bot/botManagerWindow.h>

// Std dependencies
#include <ctime>
#include <string>
#include <fstream>
#include <filesystem>

// Third party dependencies
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <glad/glad.h>
#include <fmt/core.h>

// Internal dependencies
#include <system/windowCaptureService.h>
#include <system/mouseMovementDatabase.h>
#include <system/resourceManager.h>
#include <system/inputManager.h>
#include <ml/onnxruntimeInference.h>
#include <utils.h>

// Tasks
#include <bot/tasks/findTabTask.h>
#include <bot/tasks/inventoryDropTask.h>

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

	// Pre-initialize tasks
	_tasks.push_back(new FindTabTask());
	_tasks.push_back(new InventoryDropTask());
}

BotManagerWindow::~BotManagerWindow()
{
	glDeleteTextures(1, &_frameTexId);
}

void BotManagerWindow::Run(float deltaTime)
{
	// Clear resource manager for frame
	ResourceManager& resourceManager = ResourceManager::GetInstance();
	resourceManager.RemoveAllResources();

	// Fetch a new copy of the image
	_frame = _captureService.GetLatestFrame();

	// Set frame on resource manager
	resourceManager.SetResource("Main Frame", &_frame);

	if (_isBotRunning)
	{
		if (!_mouseMovementDatabase.IsLoaded())
		{
			_mouseMovementDatabase.LoadMovements();
		}

		// Run tasks
		for (auto task : _tasks)
		{
			task->Run(deltaTime);
		}

		// Draw cursor
		cv::Point mousePos;
		_inputManager.GetMousePosition(mousePos);
		mousePos = _captureService.SystemToFrameCoordinates(mousePos, _frame);
		cv::drawMarker(_frame, mousePos, {	0,	0,	0}, cv::MARKER_CROSS, 22, 3, cv::LINE_AA);
		cv::drawMarker(_frame, mousePos, {255,255,255}, cv::MARKER_CROSS, 18, 1, cv::LINE_AA);
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

				ImGui::TextWrapped("Use this panel to configure the bot's tasks.");

				ImGui::Separator();
				if (ImGui::Button("Add Task"))
				{
					// TODO: Add task
				}

				static std::vector<float> lastTaskSizes(_tasks.size());
				lastTaskSizes.resize(_tasks.size());
				for (int i = 0; i < _tasks.size(); i++)
				{
					ImGui::PushID(i);
					{
						// TODO: Draw inputs/output validation errors
						ImGuiPanelGuard taskPanel(_tasks[i]->GetName(), ImVec2(0, lastTaskSizes[i]),
							true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

						float cursorIniY = ImGui::GetCursorPosY();
						_tasks[i]->Draw();
						lastTaskSizes[i] = (ImGui::GetCursorPosY() - cursorIniY) + 40.0f;

						if (ImGui::Button("Delete Task"))
						{
							// TODO: Delete task
						}
					}
					ImGui::PopID();
				}
			}

			// ===================================== //
			// Bot View Panel (Screen Capture + Bot) //
			// ===================================== //
			ImGui::TableNextColumn();
			{
				{
					ImGuiPanelGuard botManager("Bot Manager", { 0, 50 });

					ImGui::Text("Use this panel to control the bot.");
					static int numClasses = 8;
					static float confidenceThreshold = 0.7f;
					if (!_isBotRunning)
					{
						if (ImGui::Button("Start Bot") ||  _inputManager.IsCapsLockOn())
						{
							// Try to load all tasks
							bool sucess = true;
							for (auto task : _tasks)
							{
								if (!task->Load())
								{
									sucess = false;
									break;
								}
							}

							// Set state accordingly
							_isBotRunning = sucess;
							_inputManager.SetCapsLock(sucess);
						}
					}
					else
					{
						if (ImGui::Button("Stop Bot"))
						{
							_isBotRunning = false;
							_inputManager.SetCapsLock(false);
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

void BotManagerWindow::runMineCopperTask(float deltaTime)
{
	// Before we update the box states, we need to increment the last seen
	// time for each box state, and remove the ones that have not been seen
	for (auto it = _detectionsStates.begin(); it != _detectionsStates.end();)
	{
		DetectionBoxState& boxState = *it;
		boxState.lastSeen += deltaTime;
		if (boxState.lastSeen > 5.0f) // TODO: Make this a configurable value
		{
			// Reset clicked pointer if removed
			if (_curTargetBoxState == &boxState)
			{
				resetCurrentBoxTarget();
			}
			it = _detectionsStates.erase(it);
		}
		else
		{
			++it;
		}
	}

	// Try to map current detections to persisted box states
	// If a detection is not found, add it to the box states
	for (int i = 0; i < _detections.size(); ++i)
	{
		bool found = false;
		const auto& detection = _detections[i];
		for (auto& boxState : _detectionsStates)
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
						resetCurrentBoxTarget();
					}
				}

				boxState.box = detection;
				boxState.lastSeen = 0.0f;
				found = true;
				break;
			}
		}

		if (!found)
		{
			_detectionsStates.push_back({ detection, 0.0f });
		}
	}

	// TODO: Draw using ImGui or OpenGL (for performance),
	// and move this to a proper wrapper class per model
	// Draw rectangles on the image frame
	int i = 0;
	for (auto& state : _detectionsStates)
	{
		const auto& detection = state.box;
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
		if (detection.classId == 7) { color = cv::Scalar(0, 0, 0);			label = "Depleted";	};
		cv::rectangle(_frame, rect, color, 2);
		label += fmt::format(" ({}:{:.3f}s)", i++, state.lastSeen);
		cv::putText(_frame, label, rect.tl() - cv::Point{0, 15}, cv::HersheyFonts::FONT_HERSHEY_PLAIN, 1.0, color, 2);
	}

	// Update mouse movement database
	_mouseMovementDatabase.UpdateDatabase();

	// TEST: Fetch closes copper ore to center of screen (player) and
	// query a mouse movement from current mouse position to that point
	cv::Point playerPos(_frame.cols / 2, _frame.rows / 2);
	cv::Point closestCopperPos;
	float closestCopperRadius = 0.0f;
	float closestCopperDistance = FLT_MAX;
	DetectionBoxState* closestCopperBox = nullptr;
	for (auto& state : _detectionsStates)
	{
		// Only if state is currently visible
		if (state.lastSeen > 0.0f) continue;

		const auto& detection = state.box;
		if (detection.classId != 2) continue; // Copper

		cv::Point copperPos = detection.GetCenter();
		float distance = cv::norm(playerPos - copperPos);
		if (distance < closestCopperDistance)
		{
			closestCopperDistance = distance;
			closestCopperPos = copperPos;
			closestCopperRadius = std::min(detection.w / 2, detection.h / 2);
			closestCopperBox = &state;
		}
	}

	// If no copper ore was found (and no current target box active), return
	if (closestCopperBox == nullptr && _curTargetBoxState == nullptr)
	{
		return;
	}

	// These will be used throughout the function
	cv::Point mousePos;
	{
		_inputManager.GetMousePosition(mousePos);
		closestCopperPos = _captureService.FrameToSystemCoordinates(closestCopperPos, _frame);
	}

	// We are seeking a new target box and we found one
	if (_curTargetBoxState == nullptr && closestCopperBox != nullptr)
	{
		// Query mouse movement from current mouse
		MouseMovement bestMovement;
		_mouseMovementDatabase.QueryMovement(mousePos, closestCopperPos, closestCopperRadius * 0.85f, bestMovement, 0.0f, 1.5f);

		// If we have a movement, we can pick this box as the target and start moving
		if (bestMovement.IsValid())
		{
			_curTargetBoxState = closestCopperBox;
			_curMouseMovement = std::move(bestMovement);
			// Reset next movement so we don't play bits of it after we are done with the current one
			_nextMouseMovement.~MouseMovement();
		}
	}

	// If we have a target box we are either waiting
	// for the ore to be mined or we are moving to it
	if (_curTargetBoxState != nullptr)
	{
		// We have a valid movement, consume it
		if (_curMouseMovement.IsValid())
		{
			drawMouseMovement(_curMouseMovement, _frame);

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
						_mouseMovementDatabase.QueryMovement(mousePos, mousePos, 200.0f, _curMouseMovement, 0.0f, 0.5f);
					}
				}
				else // Not final point yet, just move the mouse
				{
					_inputManager.SetMousePosition(mousePos);
				}
			}
		}
		else // We are waiting for the ore to be mined
		{
			if (_curTargetBoxState->box.classId != 2) // Not copper anymore
			{
				// It was mined
				resetCurrentBoxTarget();
			}
			else // Still copper
			{
				// We could be still mining it, or it got collected and respawned when we had no tracking of it
				// So we use a timer to ensure that if it takes more than 5s we will be resetting the target
				if (_curTargetBoxState->lastSeen > 0.0f)
				{
					// Flag that we are using a timer, as the state of the ore is now unkown (lost tracking)
					// TODO: Remove this once we get a good enough model that doesn't lose track of the ore
					if (!_useWaitTimer)
					{
						_waitTimer = 0.0f;
						_useWaitTimer = true;
					}
				}

				if (_useWaitTimer)
				{
					_waitTimer += deltaTime;
				}
				if (_waitTimer > 10.0f)
				{
					resetCurrentBoxTarget();
				}
				else // Meanwhile we can move the mouse around just to be fancy
				{
					if (!_nextMouseMovement.IsValid()) // We may fetch a new movement
					{
						// If our cursor is already in the right spot, just be idle
						if (cv::norm(mousePos - closestCopperPos) < closestCopperRadius * 0.85f)
						{
							_mouseMovementDatabase.QueryMovement(mousePos, mousePos, 200.0f, _nextMouseMovement, 0.7f, 20.0f);
						}
						else
						{
							// If there is a closest box that's not our target, pick a movement to it
							if (closestCopperBox != _curTargetBoxState)
							{
								_mouseMovementDatabase.QueryMovement(mousePos, closestCopperPos, closestCopperRadius * 0.85f, _nextMouseMovement, 0.0f, 1.5f);
							}
							else // Otherwise we can just do some random movements
							{
								_mouseMovementDatabase.QueryMovement(mousePos, mousePos, 200.0f, _nextMouseMovement, 1.0f, 20.0f);
							}
						}
					}
					else // Play the mouse movement
					{
						drawMouseMovement(_nextMouseMovement, _frame);

						// Play the movement
						MousePoint& point = _nextMouseMovement.points[0];
						point.deltaTime -= deltaTime;
						if (point.deltaTime <= 0.0f) // Consume point
						{
							_inputManager.SetMousePosition(point.pos);
							_nextMouseMovement.points.erase(_nextMouseMovement.points.begin());
						}
					}
				}
			}
		}
	}
}

void BotManagerWindow::resetCurrentBoxTarget()
{
	_curTargetBoxState = nullptr;
	_curMouseMovement.~MouseMovement();
	_nextMouseMovement.~MouseMovement();
	_curClickState = MOUSE_CLICK_NONE;
	_useWaitTimer = false;
	_waitTimer = 0.0f;
}
