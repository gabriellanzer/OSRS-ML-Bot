#include <bot/tasks/inventoryDropTask.h>

// Third party dependencies
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <fmt/core.h>

// Internal dependencies
#include <system/resourceManager.h>
#include <bot/tasks/findTabTask.h>
#include <utils.h>

InventoryDropTask::InventoryDropTask()
{
	// Set the default model path
	const wchar_t* defaultModelPath = L"..\\..\\models\\yolov8s-osrs-inventory-ores-v1.onnx";
	const size_t len = wcslen(defaultModelPath) + 1;
	_modelPath = new wchar_t[len];
	std::memcpy(_modelPath, defaultModelPath, len * sizeof(wchar_t));
}

InventoryDropTask::~InventoryDropTask()
{
	if (_model != nullptr)
	{
		delete _model;
		_model = nullptr;
	}
	delete[] _modelPath;
}

bool InventoryDropTask::Load()
{
	if (_modelPath == nullptr) return false;

	// Load the model
	_model = new YOLOv8(18, _confidenceThreshold);
	_model->LoadModel(true, _modelPath);

	// Run a warm-up inference
	cv::Mat frame = WindowCaptureService::GetInstance().GetLatestFrame();
	_model->Inference(frame, _detectedItems);

	return true;
}

void InventoryDropTask::Run(float deltaTime)
{
	auto& resourceManager = ResourceManager::GetInstance();

	// Fetch the tab frame
	cv::Mat* tabFrame = nullptr;
	if (!resourceManager.TryGetResource(TabNames[TAB_INVENTORY], tabFrame))
	{
		return;
	}

	// Update model params
	_model->SetConfidenceThreshold(_confidenceThreshold);

	// Run inference
	_model->Inference(*tabFrame, _detectedItems);

	// Draw the detected items
	for (const auto& item : _detectedItems)
	{
		cv::Rect rect(item.x, item.y, item.w, item.h);
		cv::rectangle(*tabFrame, rect, cv::Scalar(255, 255, 255), 2);
		cv::putText(*tabFrame, fmt::format("{}", OreNames[item.classId]), rect.tl() - cv::Point{ 0, 5 }, cv::HersheyFonts::FONT_HERSHEY_PLAIN, 1.0, cv::Scalar(255, 255, 255), 2);
	}

	cv::imshow("Inventory Tab", *tabFrame);
}

void InventoryDropTask::Draw()
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
}

void InventoryDropTask::GetInputResources(std::vector<std::string>& resources)
{
	resources.push_back(TabNames[TAB_INVENTORY]);
}