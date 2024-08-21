
// Windows dependencies
#include <windows.h>

// Std dependencies
#include <iostream>
#include <vector>

// Third party dependencies
#include <opencv2/core.hpp>
// #include <opencv2/highgui.hpp>
// #include <opencv2/imgproc.hpp>

// Internal dependencies
#include <windowPicker.h>
#include <windowCapture.h>
#include <resultsWindow.h>
#include <onnxruntimeInference.h>

int main(int argc, char** argv)
{
	std::cout << "Picking a monitor to track...\n";
	std::flush(std::cout);
    auto [hdc, hmonitor] = pickMonitorDialog();
    if (!hdc)
	{
		std::cout << "ERROR! No monitor detected!\n";
		return -1;
	}

	initializeCaptureApi();

	//Model loading
	constexpr auto modelPath = L"..\\..\\models\\yolov8s-osrs-ores-v5.onnx";
	std::wcout << "Initializing YOLOv8, from path '" << modelPath << "'\n";
	std::flush(std::wcout);

	// These params vary with the model
	YOLOv8 model(8 /*8 ore categories (subjected to change)*/, 0.85);
	model.LoadModel(true, modelPath);

	// Initialize results window
	ResultsWindow resultsWindow(hmonitor);

	// Start main app loop
	cv::Mat image;
	std::vector<YoloDetectionBox> detections;
    while (resultsWindow.ShouldRun())
	{
		// image loading
		captureScreen(hdc, image);
 		if (image.empty())
		{
			std::cout << "ERROR! blank image grabbed\n";
			return -2;
		}

		// Run YOLO inference
		model.Inference(image, detections);

		// Update results window
		resultsWindow.Update(image, detections);
	}

	shutdownCaptureApi();
	return 0;
}
