#pragma once

// Std dependencies
#include <vector>
// Avoid symbol conflicts with std::min and std::max
#undef min
#undef max
#include <algorithm>

// Third party dependencies
#include <opencv2/core.hpp>

enum MouseButton
{
	LEFT_BUTTON,
	RIGHT_BUTTON,
	MIDDLE_BUTTON,
};

enum MouseClickState
{
	MOUSE_MOVE,
	MOUSE_DOWN,
	MOUSE_UP
};

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

	float GetAngle() const
	{
		if (points.size() < 2) return 0.0f;
		cv::Point diff = points.back().point - points.front().point;
		return atan2(diff.y, diff.x);
	}

	bool IsValid() const { return points.size() > 1; }

	std::vector<MousePoint> points;
	cv::Point minPoint, maxPoint;
	cv::Scalar color;
	MouseClickState clickState = MOUSE_DOWN;
	MouseButton button = LEFT_BUTTON;
};
