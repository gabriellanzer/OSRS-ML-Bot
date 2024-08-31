#include <bot/tasks/findTabTask.h>

// Std dependencies
#include <vector>

// Third party dependencies
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <nlohmann/json.hpp>
#include <imgui.h>

// Internal dependencies
#include <system/windowCaptureService.h>
#include <system/resourceManager.h>
#include <ml/onnxruntimeInference.h>
#include <utils.h>

const char* tabs[] = { "Attack Style", "Friends List", "Inventory", "Magic", "Prayer", "Quests", "Skills", "Equipments" };

FindTabTask::FindTabTask()
{
	// Set the default model path
	_modelPath = new wchar_t[64];
	const wchar_t* defaultModelPath = L"..\\..\\models\\yolov8s-osrs-ui-tabs-v1.onnx";
	std::memcpy(_modelPath, defaultModelPath, (wcslen(defaultModelPath) + 1) * sizeof(wchar_t));
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
	cv::Mat frame = WindowCaptureService::GetInstance().GetLatestFrame();

	// Update model params
	_model->SetConfidenceThreshold(_confidenceThreshold);

	// Run inference
	_model->Inference(frame, _detectedTabs);

	// Find the tab we are tracking
	for (const auto& tab : _detectedTabs)
	{
		if (tab.classId == _trackingTab)
		{
			// Extract the tab frame
			_tabFrame = frame(cv::Rect(tab.x, tab.y, tab.w, tab.h)).clone();
			break;
		}
	}

	// Set the output resource
	ResourceManager::GetInstance().SetResource("Tab Frame", &_tabFrame);
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
	ImGui::SliderFloat("##confidenceThreshold", &_confidenceThreshold, 0.0f, 1.0f);

	// ===================================== //
	// Model Configuration                   //
	// ===================================== //
	ImGui::SeparatorText("Tracking Configuration");
    ImGui::TextUnformatted("Tracking Tab:");
    ImGui::SameLine();
	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::BeginCombo("##trackingTab", tabs[_trackingTab])) {
        for (int n = 0; n < IM_ARRAYSIZE(tabs); n++) {
            bool isSelected = (_trackingTab == n);
            if (ImGui::Selectable(tabs[n], isSelected)) {
                _trackingTab = (TabClasses)n;
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
}

void FindTabTask::GetOutputResources(std::vector<std::string>& resources)
{
	resources.push_back(tabs[_trackingTab]);
}