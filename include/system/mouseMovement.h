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
	MOUSE_BUTTON_LEFT	= 0,
	MOUSE_BUTTON_RIGHT	= 1,
	MOUSE_BUTTON_MIDDLE = 2,
	MOUSE_BUTTON_COUNT	= 3
};

enum MouseClickState
{
	MOUSE_CLICK_NONE	= -1,
	MOUSE_CLICK_DOWN	= 0,
	MOUSE_CLICK_UP		= 1,
	MOUSE_CLICK_COUNT	= 3
};

struct MousePoint
{
	cv::Point pos;
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
		return cv::norm(points[0].pos - points.back().pos);
	}

	float GetAngle() const
	{
		if (points.size() < 2) return 0.0f;
		cv::Point diff = points.back().pos - points[0].pos;
		return atan2(diff.y, diff.x);
	}

	float GetTotalTime()
	{
		float totalTime = 0.0f;
		for (const auto& point : points)
		{
			totalTime += point.deltaTime;
		}
		return totalTime;
	}

	bool IsValid() const { return !points.empty(); }

	std::vector<MousePoint> points;
	cv::Point minPoint, maxPoint;
	cv::Scalar color;
};
