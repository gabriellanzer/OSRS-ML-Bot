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
#include <ml/onnxruntimeInference.h>
#include <utils.h>


BotManagerWindow::BotManagerWindow() : _captureService(WindowCaptureService::getInstance())
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

void runBotInference(cv::Mat& image, YOLOInterfaceBase* model, std::vector<YoloDetectionBox>& detections)
{
	// Run YOLO inference
	model->Inference(image, detections);

	// TODO: Draw using ImGui or OpenGL (for performance),
	// and move this to a proper wrapper class per model
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

void BotManagerWindow::Run()
{
	// Fetch a new copy of the image
	if (ImGui::Begin("Bot", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
	{
		_frame = _captureService.GetLatestFrame();
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
							runBotInference(_frame, _model, _detections);
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

					if (_isBotRunning)
					{
						runBotInference(_frame, _model, _detections);
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
