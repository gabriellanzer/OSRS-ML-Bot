#pragma once

// Windows dependencies
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
	static WindowCaptureService& getInstance();

	void StartCapture(HDC srcHdc);
	void StopCapture();
	bool IsCapturing() const;

	cv::Mat GetLatestFrame();

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
};