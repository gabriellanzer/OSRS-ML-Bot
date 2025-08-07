#pragma once

// Std dependencies
#include <vector>

// Third party dependencies
#include <opencv2/core.hpp>

// Internal dependencies
#include <system/mouseMovement.h>
#include <ml/onnxruntimeInference.h>
#include <bot/ibotWindow.h>

struct DetectionBoxState
{
	DetectionBox box;
	float lastSeen;
};

class BotManagerWindow : public IBotWindow
{
public:
	BotManagerWindow(GLFWwindow* window);
	~BotManagerWindow();

	virtual void Run(float deltaTime) override;

private:
	void runMineCopperTask(float deltaTime);
	void resetCurrentBoxTarget();

	// Helper references
	class InputManager& _inputManager;
	class WindowCaptureService& _captureService;
	class MouseMovementDatabase& _mouseMovementDatabase;

	// Tasks
	std::vector<class IBotTask*> _tasks;

	cv::Mat _frame;
	uint32_t _frameTexId;

	bool _isBotRunning = false;

	// TODO: move this to a task
	std::vector<DetectionBox> _detections;
	std::list<DetectionBoxState> _detectionsStates;
	bool _useWaitTimer = false;
	float _waitTimer = 0.0f;
	MouseMovement _curMouseMovement;
	MouseMovement _nextMouseMovement;
	MouseClickState _curClickState = MOUSE_CLICK_NONE;
	DetectionBoxState* _curTargetBoxState = nullptr;
};