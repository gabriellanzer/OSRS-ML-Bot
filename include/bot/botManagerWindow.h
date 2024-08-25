#pragma once

// Std dependencies
#include <vector>

// Third party dependencies
#include <opencv2/core.hpp>

// Internal dependencies
#include <ml/onnxruntimeInference.h>
#include <bot/ibotwindow.h>

class BotManagerWindow : public IBotWindow
{
public:
	BotManagerWindow(GLFWwindow* window);
	~BotManagerWindow();

	virtual void Run(float deltaTime) override;

private:
	void runBotInference(cv::Mat& frame);

	// Helper references
	class InputManager& _mouseTracker;
	class WindowCaptureService& _captureService;
	class MouseMovementDatabase& _mouseMovementDatabase;

	// Internal state
	wchar_t* _modelPath = nullptr;
	YOLOInterfaceBase* _model = nullptr;

	cv::Mat _frame;
	uint32_t _frameTexId;

	bool _isBotRunning = false;
	std::vector<YoloDetectionBox> _detections;
};