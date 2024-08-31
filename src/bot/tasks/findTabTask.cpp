#include <bot/tasks/findTabTask.h>

// Std dependencies
#include <vector>

// Third party dependencies
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <nlohmann/json.hpp>
#include <fmt/core.h>
#include <imgui.h>

// Internal dependencies
#include <system/windowCaptureService.h>
#include <system/resourceManager.h>
#include <ml/onnxruntimeInference.h>
#include <utils.h>

FindTabTask::FindTabTask()
{
	// Set the default model path
	const wchar_t* defaultModelPath = L"..\\..\\models\\yolov8s-osrs-ui-tabs-v1.onnx";
	const size_t len = wcslen(defaultModelPath) + 1;
	_modelPath = new wchar_t[len];
	std::memcpy(_modelPath, defaultModelPath, len * sizeof(wchar_t));
}

FindTabTask::~FindTabTask()
{
	if (_model != nullptr)
	{
		delete _model;
		_model = nullptr;
	}
	delete[] _modelPath;
}

bool FindTabTask::Load()
{
	if (_modelPath == nullptr) return false;

	// Load the model
	_model = new YOLOv8(8, _confidenceThreshold);
	_model->LoadModel(true, _modelPath);

	// Run a warm-up inference
	cv::Mat frame = WindowCaptureService::GetInstance().GetLatestFrame();
	_model->Inference(frame, _detectedTabs);

	return true;
}

void FindTabTask::Run(float deltaTime)
{
	// Fetch a new copy of the image (can't have any drawing on it)
	cv::Mat* frame;
	ResourceManager::GetInstance().TryGetResource("Main Frame", frame);

	// Update model params
	_model->SetConfidenceThreshold(_confidenceThreshold);

	// Run inference
	_model->Inference(*frame, _detectedTabs);

	// Filter out the detections that overlap
	size_t detectionCount = _detectedTabs.size();
	for (int i = 0; i < detectionCount; ++i)
	{
		YoloDetectionBox& curBox = _detectedTabs[i];
		for (int j = i + 1; j < _detectedTabs.size(); j++)
		{
			YoloDetectionBox& otherBox = _detectedTabs[j];
			if (curBox.IsSimilar(otherBox, 0.95f))
			{
				// Skip if class is different
				if (curBox.classId != otherBox.classId) continue;

				// Merge the two detections in current
				curBox = curBox.Merge(otherBox);

				// Swap with last and pop
				_detectedTabs[j] = _detectedTabs.back();
				_detectedTabs.pop_back();

				// Prevent j increment to check the new box
				--j;
			}
		}
	}

	// Take screenshot and export detection labels
	if (_shouldOverrideClass)
	{
		for (auto& tab : _detectedTabs)
		{
			tab.classId = _overrideClass;
		}
	}
	if (_exportDetection)
	{
		_exportDetection = false;
		exportDetections(*frame, _detectedTabs);
		return;
	}

	// Find the tab we are tracking
	for (const auto& tab : _detectedTabs)
	{
		cv::Scalar color = cv::Scalar(130, 130, 130);
		if (tab.classId == _trackingTab)
		{
			// Extract the tab frame
			_tabFrame = (*frame)(cv::Rect(tab.x, tab.y, tab.w, tab.h)).clone();
			color = cv::Scalar(255, 255, 255);
		}

		cv::rectangle(*frame, cv::Rect(tab.x, tab.y, tab.w, tab.h), color, 2);
		cv::putText(*frame, fmt::format("{}", TabNames[tab.classId]), cv::Point(tab.x, tab.y - 5), cv::HersheyFonts::FONT_HERSHEY_PLAIN, 1.0, color, 2);
	}

	// Set the output resource
	if (_tabFrame.empty())
	{
		ResourceManager::GetInstance().RemoveResource(TabNames[_trackingTab]);
	}
	else
	{
		ResourceManager::GetInstance().SetResource(TabNames[_trackingTab], &_tabFrame);
	}
}

void FindTabTask::Draw()
{
	// ===================================== //
	// Model Configuration                   //
	// ===================================== //
	ImGui::SeparatorText("Model Configuration");

	ImGui::TextUnformatted("Model Path:");
	ImGui::SameLine();
	drawFilePicker("##modelPath", "Click to select model path...", _modelPath);

	ImGui::TextUnformatted("Confidence Threshold:");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
	ImGui::SliderFloat("##confidenceThreshold", &_confidenceThreshold, 0.05f, 1.0f);

	// ===================================== //
	// Tracking Configuration                //
	// ===================================== //
	ImGui::SeparatorText("Tracking Configuration");
    ImGui::TextUnformatted("Tracking Tab:");
    ImGui::SameLine();
	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::BeginCombo("##trackingTab", TabNames[_trackingTab])) {
        for (int n = 0; n < IM_ARRAYSIZE(TabNames); n++) {
            bool isSelected = (_trackingTab == n);
            if (ImGui::Selectable(TabNames[n], isSelected)) {
                _trackingTab = (TabClasses)n;
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

	// ===================================== //
	// Model Fine-Tuning                     //
	// ===================================== //
	if (ImGui::CollapsingHeader("Model Fine-Tuning"))
	{
		ImGui::SeparatorText("Model Fine-Tuning");
		if (ImGui::Button("Export Detections"))
		{
			_exportDetection = true;
		}
		ImGui::TextUnformatted("Override Classification:");
		ImGui::SameLine();
		ImGui::Checkbox("##shouldOverrideClass", &_shouldOverrideClass);
		ImGui::BeginDisabled(!_shouldOverrideClass);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
		if (ImGui::BeginCombo("##overrideClass", TabNames[_overrideClass])) {
			for (int n = 0; n < IM_ARRAYSIZE(TabNames); n++) {
				bool isSelected = (_overrideClass == n);
				if (ImGui::Selectable(TabNames[n], isSelected)) {
					_overrideClass = (TabClasses)n;
				}
				if (isSelected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		ImGui::EndDisabled();
	}
}

void FindTabTask::GetOutputResources(std::vector<std::string>& resources)
{
	resources.push_back(TabNames[_trackingTab]);
}