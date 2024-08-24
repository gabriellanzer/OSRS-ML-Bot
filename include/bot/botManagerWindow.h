#pragma once

// Std dependencies
#include <vector>

// Third party dependencies
#include <opencv2/core.hpp>

// Internal dependencies
#include <ml/onnxruntimeInference.h>
#include <bot/ibotwindow.h>

// Forward declarations
class WindowCaptureService;

class BotManagerWindow : public IBotWindow
{
public:
	BotManagerWindow(GLFWwindow* window);
	~BotManagerWindow();

	virtual void Run(float deltaTime) override;

private:
	// Helper references
	WindowCaptureService& _captureService;

	// Internal state
	wchar_t* _modelPath = nullptr;
	YOLOInterfaceBase* _model = nullptr;

	cv::Mat _frame;
	uint32_t _frameTexId;

	bool _isBotRunning = false;
	std::vector<YoloDetectionBox> _detections;
};