
// Windows dependencies
#include <windows.h>

// Third party dependencies
#include <opencv2/core.hpp>

void initializeCaptureApi();
void shutdownCaptureApi();
void captureScreen(HDC srcHdc, cv::Mat& outMat);
