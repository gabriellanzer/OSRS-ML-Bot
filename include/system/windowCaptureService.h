#pragma once

// Windows dependencies
#define NOMINMAX
#include <windows.h>

// Std dependencies
#include <atomic>
#include <mutex>
#include <thread>

// Third party dependencies
#include <opencv2/core.hpp>

class WindowCaptureService
{
  public:
	static WindowCaptureService& GetInstance()
	{
		static WindowCaptureService instance;
		return instance;
	}

	void StartCapture(HDC srcHdc, const char* adapterName);
	void StopCapture();
	bool IsCapturing() const;

	cv::Mat GetLatestFrame();
	inline cv::Point SystemToFrameCoordinates(const cv::Point& point, const cv::Mat& frame) const;
	inline cv::Point FrameToSystemCoordinates(const cv::Point& point, const cv::Mat& frame) const;
	std::pair<cv::Point, cv::Point> GetCaptureDimensions() const { return { _captureMin, _captureMax }; }

  private:
	WindowCaptureService();
	~WindowCaptureService();

	WindowCaptureService(const WindowCaptureService&) = delete;
	WindowCaptureService& operator=(const WindowCaptureService&) = delete;

	void captureLoop(HDC srcHdc);
	void captureScreen(HDC srcHdc, cv::Mat* outMat);

	std::thread _captureThread;
	std::atomic<bool> _capturing;
	std::mutex _frameMutex;
	cv::Mat* _frontFrame;
	cv::Mat* _backFrame;
	HDC _srcHdc;
	cv::Point _captureMin, _captureMax;
};

inline cv::Point WindowCaptureService::SystemToFrameCoordinates(const cv::Point& point, const cv::Mat& frame) const
{
	// Clamp to windows coordinates
	cv::Point pointClamped = point;
	pointClamped.x = std::clamp(point.x, _captureMin.x, _captureMax.x - 1);
	pointClamped.y = std::clamp(point.y, _captureMin.x, _captureMax.y - 1);

	// Wrap around if negative
	while (pointClamped.x < 0) pointClamped.x += frame.cols;
	while (pointClamped.y < 0) pointClamped.y += frame.rows;

	return pointClamped;
}

inline cv::Point WindowCaptureService::FrameToSystemCoordinates(const cv::Point& point, const cv::Mat& frame) const
{
	// Clamp to frame coordinates
	cv::Point pointClamped = point;
	pointClamped.x = std::clamp(point.x, 0, frame.cols - 1);
	pointClamped.y = std::clamp(point.y, 0, frame.rows - 1);

	// Offset by window coordinates
	pointClamped.x += _captureMin.x;
	pointClamped.y += _captureMin.y;

	return pointClamped;
}