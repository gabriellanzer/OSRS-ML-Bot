#pragma once

// Std dependencies
#include <vector>

// Third party dependencies
#include <opencv2/core.hpp>

// Internal dependencies
#include <bot/ibotwindow.h>

struct MouseMovement
{
	std::vector<cv::Point> points;
};

class TrainingLabWindow : public IBotWindow
{
  public:
	TrainingLabWindow();
	~TrainingLabWindow();

	virtual void Run() override;

  private:
	// Helper references
	class WindowCaptureService& _captureService;
	class MouseTracker* _mouseTracker;

	// Internal state
	cv::Mat _frame;
	uint32_t _frameTexId;

	int _mousePosX,  _mousePosY;
	int _mouseDownX, _mouseDownY;
	int _mouseUpX,   _mouseUpY;

	bool _captureMouseMovement = false;
	std::vector<MouseMovement> _mouseMovements;
};