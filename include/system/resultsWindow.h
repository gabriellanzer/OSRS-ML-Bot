#pragma once
#define NOMINMAX
#include <windows.h>

#include <opencv2/core.hpp>
#include <ml/onnxruntimeInference.h>

class ResultsWindow
{
  public:
	ResultsWindow(HMONITOR captureMonitor);
	~ResultsWindow();

	bool ShouldRun() { return _hwnd != nullptr; };
	void Update(cv::Mat& frame, std::vector<YoloDetectionBox>& detectionBoxes);

  private:
	HWND _hwnd;
	HMONITOR _monitor;
};