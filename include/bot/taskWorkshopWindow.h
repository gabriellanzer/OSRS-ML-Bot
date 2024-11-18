#pragma once

// Std dependencies
#include <vector>

// Third party dependencies
#include <opencv2/core.hpp>

// Internal dependencies
#include <bot/ibotWindow.h>
#include <system/mouseMovement.h>

class TaskWorkshopWindow : public IBotWindow
{
  public:
	TaskWorkshopWindow(GLFWwindow* window);
	~TaskWorkshopWindow();

	virtual void Run(float deltaTime) override;

  private:
	// Helper references
	class WindowCaptureService& _captureService;
	class MouseMovementDatabase& _mouseMovementDatabase;
	class InputManager& _inputManager;

	// Internal state
	cv::Mat _frame;
	uint32_t _frameTexId;

	float _samePosThreshold = 2.0f;
	cv::Point _mousePos;
	cv::Point _mouseDown;
	cv::Point _mouseUp;

	bool _drawMouseMovements = false;
	bool _captureMouseMovement = false;
	bool _playbackMouseMovement = false;
	MouseMovement* _curMouseMovement = nullptr;
	MouseMovement* _selMouseMovement = nullptr;
	MouseMovement* _hovMouseMovement = nullptr;
	std::vector<MouseMovement> _playbackMouseMovements;
	MouseClickState _playbackClickState = MOUSE_CLICK_NONE;
};