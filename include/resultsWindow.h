#pragma once
#include <windows.h>

#include <opencv2/core.hpp>
#include <onnxruntimeInference.h>

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