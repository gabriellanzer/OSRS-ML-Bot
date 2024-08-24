#pragma once

// Std dependencies
#include <vector>

// Third party dependencies
#include <opencv2/core.hpp>

// Internal dependencies
#include <bot/ibotwindow.h>

struct MousePoint
{
	cv::Point point;
	float deltaTime;
};

struct MouseMovement
{
	void AddPoint(cv::Point point, float deltaTime)
	{
		points.push_back({point, deltaTime});

		if (points.size() == 1)
		{
			minPoint = point;
			maxPoint = point;
		}
		else
		{
			minPoint = cv::Point(std::min(minPoint.x, point.x), std::min(minPoint.y, point.y));
			maxPoint = cv::Point(std::max(maxPoint.x, point.x), std::max(maxPoint.y, point.y));
		}
	}

	float IniEndDistance() const
	{
		if (points.size() < 2) return 0.0f;
		return cv::norm(points.front().point - points.back().point);
	}

	std::vector<MousePoint> points;
	cv::Point minPoint, maxPoint;
	cv::Scalar color;
	bool isMouseDown = true;
};

class TrainingLabWindow : public IBotWindow
{
  public:
	TrainingLabWindow(GLFWwindow* window);
	~TrainingLabWindow();

	virtual void Run(float deltaTime) override;

  private:
	// Helper references
	class WindowCaptureService& _captureService;
	class MouseTracker* _mouseTracker;

	// Internal state
	cv::Mat _frame;
	uint32_t _frameTexId;

	float _internalTimer = 0.0f;
	float _samePosThreshold = 2.0f;
	int _mousePosX,  _mousePosY;
	int _mouseDownX, _mouseDownY;
	int _mouseUpX,   _mouseUpY;

	bool _drawMouseMovements = false;
	bool _captureMouseMovement = false;
	bool _playbackMouseMovement = false;
	MouseMovement* _curMouseMovement = nullptr;
	MouseMovement* _selMouseMovement = nullptr;
	std::vector<MouseMovement> _mouseMovements;
	std::vector<MouseMovement> _playbackMouseMovements;
};