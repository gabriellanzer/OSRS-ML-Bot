#pragma once

// Std dependencies
#include <vector>

// Third party dependencies
#include <opencv2/core.hpp>

// Internal dependencies
#include <system/mouseMovement.h>
#include <ml/onnxruntimeInference.h>
#include <bot/ibotwindow.h>

struct DetectionBoxState
{
	YoloDetectionBox box;
	float lastSeen;
};

class BotManagerWindow : public IBotWindow
{
public:
	BotManagerWindow(GLFWwindow* window);
	~BotManagerWindow();

	virtual void Run(float deltaTime) override;

private:
	void runBotInference(float deltaTime, cv::Mat& frame);

	// Helper references
	class InputManager& _inputManager;
	class WindowCaptureService& _captureService;
	class MouseMovementDatabase& _mouseMovementDatabase;

	// Internal state
	wchar_t* _modelPath = nullptr;
	YOLOInterfaceBase* _model = nullptr;

	cv::Mat _frame;
	uint32_t _frameTexId;

	bool _isBotRunning = false;
	std::vector<YoloDetectionBox> _detections;
	std::vector<DetectionBoxState*> _boxStatesMapping;
	std::vector<DetectionBoxState> _boxStates;

	// TODO: move this to a task
	MouseMovement _curMouseMovement;
	MouseClickState _curClickState = MOUSE_CLICK_NONE;
	DetectionBoxState* _curTargetBoxState = nullptr;
};