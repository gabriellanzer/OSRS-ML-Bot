#pragma once

// Std dependencies
#include <vector>

// Third party dependencies
#include <opencv2/core.hpp>

// Internal dependencies
#include <bot/ibotwindow.h>
#include <system/mouseMovement.h>

class TrainingLabWindow : public IBotWindow
{
  public:
	TrainingLabWindow(GLFWwindow* window);
	~TrainingLabWindow();

	virtual void Run(float deltaTime) override;

  private:
	// Helper references
	class WindowCaptureService& _captureService;
	class MouseMovementDatabase& _mouseMovementDatabase;
	class MouseTracker* _mouseTracker;

	// Internal state
	cv::Mat _frame;
	uint32_t _frameTexId;

	float _internalTimer = 0.0f;
	float _samePosThreshold = 2.0f;
	cv::Point _mousePos;
	cv::Point _mouseDown;
	cv::Point _mouseUp;

	bool _drawMouseMovements = false;
	bool _captureMouseMovement = false;
	bool _playbackMouseMovement = false;
	MouseMovement* _curMouseMovement = nullptr;
	MouseMovement* _selMouseMovement = nullptr;
	std::vector<MouseMovement> _playbackMouseMovements;
};