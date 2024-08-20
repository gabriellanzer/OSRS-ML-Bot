
#include <windows.h>
#include <wingdi.h>
#include <GdiPlus.h>

#include <stdlib.h>
#include <iostream>
#include <vector>
#include <chrono>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "ws2_32.lib")

#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include <onnxruntime_inference.h>


struct DetectionBox
{
    float x, y, w, h;
    cv::Scalar color;
    std::string label;
};

struct ResultData {
    cv::Mat* image;
    std::vector<DetectionBox>* detections;
};

std::pair<HDC, HMONITOR> pickMonitor();
void captureScreen(HDC srcHdc, cv::Mat& outMat);
HWND showResultsWindow(cv::Mat& image, std::vector<DetectionBox>& detections, HMONITOR captureMonitor);

double fps = 0.0;

int main(int argc, char** argv)
{
	// Initialize GDI+
	Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

	std::cout << "Picking a monitor to track...\n";
	std::flush(std::cout);
    auto [hdc, hmonitor] = pickMonitor();
    if (!hdc)
	{
		std::cout << "ERROR! No monitor detected!\n";
		return -1;
	}

	std::cout << "Initializing ONNX runtime (Version " << Ort::GetVersionString() << ")\n";

	// Check providers
	auto providers = Ort::GetAvailableProviders();
	std::cout << "Available providers:\n";
	for (auto provider : providers) {
		std::cout << provider << std::endl;
	}

	// image loading
	cv::Mat image;
	captureScreen(hdc, image);
	if (image.empty())
	{
		std::cout << "ERROR! blank image grabbed\n";
		return -2;
	}

	//Model loading
	OnnxENV onnxEnv;

	constexpr auto modelPath = L"..\\..\\models\\yolov8s-osrs-ores-v5.onnx";
	std::wcout << "Initializing YOLOv8, from path '" << modelPath << "'\n";
	std::flush(std::wcout);

	YOLOv8 model;
	std::vector<int64_t> inputDims = { 1, 3, 640, 640 };		// Input dimensions
	double scaleW = (float)image.cols / (float)640;
	double scaleH = (float)image.rows / (float)640;
	model.SetInputDemensions(inputDims);
	model.SetSessionOptions(true);
	model.LoadWeights(&onnxEnv, modelPath);	// Load weight and create session
	std::vector<const char*> inputNodeNames = { "images" }; 	// Input node names
	model.SetInputNodeNames(&inputNodeNames);
	std::vector<const char*> outputNodeNames = { "output0" };	// Output node names
	model.SetOutputNodeNames(&outputNodeNames);

	// Inference
	std::cout << "Running warmup inference...";
	std::vector<Ort::Value> outputTensor;
	// model.Inference(image, outputTensor);
	std::cout << "Done!\n";
	std::cout << "Model loaded successfully!\n";

	// Initialize results window
	std::vector<DetectionBox> detections;
    HWND hwnd = showResultsWindow(image, detections, hmonitor);

	std::chrono::steady_clock::time_point lastTime = std::chrono::steady_clock::now();
	int frameCount = 0;

	// Start main app loop
	MSG msg = {};
    while (true)
	{
		// FPS calculation
		auto currentTime = std::chrono::steady_clock::now();
		std::chrono::duration<double> elapsedTime = currentTime - lastTime;
		frameCount++;

		if (elapsedTime.count() >= 1.0) {
			fps = frameCount / elapsedTime.count();
			frameCount = 0;
			lastTime = currentTime;
		}

		// image loading
		captureScreen(hdc, image);
 		if (image.empty())
		{
			std::cout << "ERROR! blank image grabbed\n";
			return -2;
		}

		// Inference
		int numOutputs = model.Inference(image, outputTensor);

		std::vector<int64_t> outputTensor0Shape = outputTensor.front().GetTensorTypeAndShapeInfo().GetShape();
		float* allData0 = outputTensor.front().GetTensorMutableData<float>();
		// Each model has different output format, thus the output should be parsed accordingly
		// For yolov10m the output is (1x300x6), the six outputs of the v10 model are (x, y, w, h, confidence, classId)
		// Transpose: [bs, features, preds_num]=>[bs, preds_num, features]
		cv::Mat output0 = cv::Mat(cv::Size((int)outputTensor0Shape[2], (int)outputTensor0Shape[1]), CV_32F, allData0).t();

		// 8 - number of classes (adamant, coal, copper, iron, mithril, silver, tin, wasted)
		int classNamesNum = 8;

		// 5 - your default number of geometry (rect/obb) parameters {x, y, x, y, (rot)}
		int geomStride = 4;
		int dataWidth = classNamesNum + geomStride;
		int rows = output0.rows;
		float* pData = (float*)output0.data;

        // Update detections
        detections.clear();
		double maxConfidence = 0.0;
		for (int r = 0; r < rows; ++r)
		{
			cv::Mat scores(1, classNamesNum, CV_32FC1, pData + geomStride);
			cv::Point classId;
			double maxConf;
			minMaxLoc(scores, nullptr, &maxConf, nullptr, &classId);

			if (maxConf > maxConfidence)
			{
				maxConfidence = maxConf;
			}

			if (maxConf > 0.85)
			{
				float xOrig = pData[0];
				float yOrig = pData[1];
				float wOrig = pData[2];
				float hOrig = pData[3];
				float x = (xOrig - wOrig * 0.5) * scaleW;
				float y = (yOrig - hOrig * 0.5) * scaleH;
				float w = wOrig * scaleW;
				float h = hOrig * scaleH;

				std::string label;
				cv::Scalar color = cv::Scalar(0, 0, 0);
				if (classId.x == 0) { color = cv::Scalar(0, 128, 0);		label = "Adamant";	};
				if (classId.x == 1) { color = cv::Scalar(79, 69, 54);		label = "Coal";		};
				if (classId.x == 2) { color = cv::Scalar(51, 115, 184); 	label = "Copper";	};
				if (classId.x == 3) { color = cv::Scalar(34, 34, 178);		label = "Iron";		};
				if (classId.x == 4) { color = cv::Scalar(180, 130, 70); 	label = "Mithril";	};
				if (classId.x == 5) { color = cv::Scalar(192, 192, 192);	label = "Silver";	};
				if (classId.x == 6) { color = cv::Scalar(193, 205, 205);	label = "Tin";		};
				if (classId.x == 7) { color = cv::Scalar(0, 0, 0);			label = "Wasted";	};

				detections.push_back({ x, y, w, h, color, label });
			}
			pData += dataWidth; // next pred
		}

		outputTensor.clear();

		// This also has to be set every frame
		ResultData resultData = { &image, &detections };
		SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)&resultData);

		// Update window content
        InvalidateRect(hwnd, NULL, TRUE);

        // Process window messages
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

		// Break loop if the window was closed
		if (msg.message == WM_QUIT)
		{
			std::cout << "Exiting...\n";
			DestroyWindow(hwnd);
			break;
		}
	}

	// Shutdown GDI+
	DeleteDC(hdc);
	Gdiplus::GdiplusShutdown(gdiplusToken);

	return 0;
}

struct MonitorInfo {
    HMONITOR hMonitor;
    MONITORINFOEX monitorInfoEx;
	DISPLAY_DEVICE displayDevice;
	HWND hwnd;
	bool hovered = false;
};

BOOL CALLBACK monitorEnumFunc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
    std::vector<MonitorInfo>* monitors = reinterpret_cast<std::vector<MonitorInfo>*>(dwData);
    MonitorInfo info;
    info.hMonitor = hMonitor;
    info.monitorInfoEx.cbSize = sizeof(MONITORINFOEX);
    GetMonitorInfo(hMonitor, &info.monitorInfoEx);

    DISPLAY_DEVICE displayDevice;
    displayDevice.cb = sizeof(DISPLAY_DEVICE);
    EnumDisplayDevices(info.monitorInfoEx.szDevice, 0, &displayDevice, 0);
    info.displayDevice = displayDevice;

    monitors->push_back(info);
    return TRUE;
}

LRESULT CALLBACK monitorPickerWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    MonitorInfo* monitor = reinterpret_cast<MonitorInfo*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    switch (uMsg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (monitor) {
                RECT rect = {0, 0, monitor->monitorInfoEx.rcMonitor.right - monitor->monitorInfoEx.rcMonitor.left,
								   monitor->monitorInfoEx.rcMonitor.bottom - monitor->monitorInfoEx.rcMonitor.top};
                SetTextColor(hdc, RGB(255, 255, 255));
                SetBkMode(hdc, TRANSPARENT);

				const char* deviceName = monitor->monitorInfoEx.szDevice;
				// Skip to after last backslash
				for (const char* p = deviceName; *p; ++p) { if (*p == '\\' && *(p+1)) deviceName = p + 1; }

				std::string deviceNameStr(deviceName);
				deviceNameStr = "Capture " + deviceNameStr;
                DrawText(hdc, deviceNameStr.c_str(), -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
            EndPaint(hwnd, &ps);
            break;
        }
		case WM_ERASEBKGND: {
            HDC hdc = (HDC)wParam;
            RECT rect;
            GetClientRect(hwnd, &rect);
            HBRUSH brush = CreateSolidBrush(monitor->hovered ? RGB(20, 20, 20) : RGB(1, 1, 1));
            FillRect(hdc, &rect, brush);
            DeleteObject(brush);
            return 1; // Indicate that the background has been erased
        }
        case WM_LBUTTONDOWN: {
            if (monitor) {
                PostQuitMessage(reinterpret_cast<WPARAM>(monitor->hMonitor));
            }
            break;
        }
        case WM_MOUSEMOVE: {
			// Set the window to semi-transparent white
			SetLayeredWindowAttributes(hwnd, RGB(255, 255, 255), 230, LWA_ALPHA);
			if (!monitor->hovered)
			{
				monitor->hovered = true;
				InvalidateRect(hwnd, NULL, TRUE);
			}

			// Set the cursor to the standard arrow
			SetCursor(LoadCursor(NULL, IDC_ARROW));

            TRACKMOUSEEVENT tme = {};
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);
            break;
        }
        case WM_MOUSELEAVE: {
			if (monitor->hovered)
			{
				monitor->hovered = false;
				InvalidateRect(hwnd, NULL, TRUE);
			}

			// Set the cursor to the standard arrow
			SetCursor(LoadCursor(NULL, IDC_ARROW));

			// Set the window to almost black semi-transparent
			SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 80, LWA_COLORKEY | LWA_ALPHA);
            break;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

void drawDisplayNames(std::vector<MonitorInfo>& monitors) {
    HINSTANCE hInstance = GetModuleHandle(NULL);
    const char CLASS_NAME[] = "MonitorOverlay";

    WNDCLASS wc = {};
    wc.lpfnWndProc = monitorPickerWindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);

    RegisterClass(&wc);

    for (auto& monitor : monitors) {
        HWND hwnd = CreateWindowEx(
            WS_EX_LAYERED | WS_EX_TOPMOST,
            CLASS_NAME,
            NULL,
            WS_POPUP,
            monitor.monitorInfoEx.rcMonitor.left,
            monitor.monitorInfoEx.rcMonitor.top,
            monitor.monitorInfoEx.rcMonitor.right - monitor.monitorInfoEx.rcMonitor.left,
            monitor.monitorInfoEx.rcMonitor.bottom - monitor.monitorInfoEx.rcMonitor.top,
            NULL,
            NULL,
            hInstance,
            NULL
        );

        SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 80, LWA_COLORKEY | LWA_ALPHA); // Initial state: almost black semi-transparent
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&monitor));
        monitor.hwnd = hwnd;
        ShowWindow(hwnd, SW_SHOW);
    }
}

std::pair<HDC, HMONITOR> pickMonitor() {
    std::vector<MonitorInfo> monitors;
    EnumDisplayMonitors(NULL, NULL, monitorEnumFunc, reinterpret_cast<LPARAM>(&monitors));

    if (monitors.empty()) {
        std::cerr << "No monitors found." << std::endl;
        return { NULL, NULL };
    }

	drawDisplayNames(monitors);

    // Wait for user to click on a window
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

	HDC hdc = NULL;
	HMONITOR hmonitor = NULL;

	// Fetch the device context of the monitor that was clicked
	if (msg.message == WM_QUIT) {
		for (const auto& monitor : monitors) {
			if (reinterpret_cast<HMONITOR>(msg.wParam) == monitor.hMonitor) {
				hdc = CreateDC(monitor.monitorInfoEx.szDevice, NULL, NULL, NULL);
				hmonitor = monitor.hMonitor;
				break;
			}
		}
	}

	// Destroy the window used for picking
	for (auto& monitor : monitors)
	{
		if (monitor.hwnd)
		{
			DestroyWindow(monitor.hwnd);
		}
	}

    return { hdc, hmonitor };
}

void captureScreen(HDC srcHdc, cv::Mat& outMat)
{
    int height = GetDeviceCaps(srcHdc, VERTRES);
    int width = GetDeviceCaps(srcHdc, HORZRES);

    HDC memHdc = CreateCompatibleDC(srcHdc);
    HBITMAP memBit = CreateCompatibleBitmap(srcHdc, width, height);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(memHdc, memBit);

	auto iniTime = std::chrono::steady_clock::now();
    BitBlt(memHdc, 0, 0, width, height, srcHdc, 0, 0, SRCCOPY);

    BITMAPINFOHEADER bi;
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = width;
    bi.biHeight = -height; // Negative height to indicate top-down bitmap
    bi.biPlanes = 1;
    bi.biBitCount = 24; // 24-bit bitmap (8 bits per channel)
    bi.biCompression = BI_RGB;
    bi.biSizeImage = 0;
    bi.biXPelsPerMeter = 0;
    bi.biYPelsPerMeter = 0;
    bi.biClrUsed = 0;
    bi.biClrImportant = 0;

    static std::vector<BYTE> buffer(width * height * 3); // Buffer to hold bitmap data
    GetDIBits(memHdc, memBit, 0, height, buffer.data(), (BITMAPINFO*)&bi, DIB_RGB_COLORS);
	auto endTime = std::chrono::steady_clock::now();
	std::chrono::duration<double> durTime = endTime - iniTime;
	// std::cout << "Capture time: " << durTime.count() * 1000.0 << "ms\n";

	// Initialize mat if size does not match
	if (outMat.empty() || outMat.cols != width || outMat.rows != height)
	{
		outMat = cv::Mat(height, width, CV_8UC3);
	}

	// Copy buffer data to cv::Mat using memcpy
	for (int y = 0; y < height; ++y) {
		BYTE* srcRow = buffer.data() + y * width * 3;
		cv::Vec3b* dstRow = outMat.ptr<cv::Vec3b>(y);
		memcpy(dstRow, srcRow, width * 3);
	}

    SelectObject(memHdc, hOldBitmap);
    DeleteObject(memHdc);
    DeleteObject(memBit);
}

LRESULT CALLBACK resultWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    ResultData* resultData = (ResultData*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (uMsg) {
		case WM_CREATE: {
            // Draw the initial text
            RECT rect;
            GetClientRect(hwnd, &rect);

            // Fill the background with black
            HBRUSH blackBrush = CreateSolidBrush(RGB(0, 0, 0));
            HDC hdc = GetDC(hwnd);
            FillRect(hdc, &rect, blackBrush);
            DeleteObject(blackBrush);

            // Draw the initializing text
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 255, 255));
            const char* text = "Initializing YOLO-v8 model...";
            DrawText(hdc, text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            ReleaseDC(hwnd, hdc);
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
			if (!resultData)
			{
				RECT rect;
				GetClientRect(hwnd, &rect);

				// Fill the background with black
				HBRUSH blackBrush = CreateSolidBrush(RGB(0, 0, 0));
				FillRect(hdc, &rect, blackBrush);
				DeleteObject(blackBrush);

				// Draw the initializing text
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, RGB(255, 255, 255));
                const char* text = "Initializing YOLO-v8 model...";
                DrawText(hdc, text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
			}
			else if (resultData && resultData->image && !resultData->image->empty())
			{
                BITMAPINFO bmi = {0};
                bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
                bmi.bmiHeader.biWidth = resultData->image->cols;
                bmi.bmiHeader.biHeight = -resultData->image->rows; // Negative to indicate top-down bitmap
                bmi.bmiHeader.biPlanes = 1;
                bmi.bmiHeader.biBitCount = 24;
                bmi.bmiHeader.biCompression = BI_RGB;

                StretchDIBits(hdc, 0, 0, resultData->image->cols, resultData->image->rows, 0, 0, resultData->image->cols, resultData->image->rows, resultData->image->data, &bmi, DIB_RGB_COLORS, SRCCOPY);

                for (const auto& detection : *resultData->detections) {
                    RECT rect = { (LONG)detection.x, (LONG)detection.y, (LONG)(detection.x + detection.w), (LONG)(detection.y + detection.h) };
                    HBRUSH brush = CreateSolidBrush(RGB(detection.color[2], detection.color[1], detection.color[0]));
                    FrameRect(hdc, &rect, brush);
                    DeleteObject(brush);

					// Draw the label
					SetBkMode(hdc, TRANSPARENT);
					SetTextColor(hdc, RGB(detection.color[2], detection.color[1], detection.color[0]));
					TextOut(hdc, rect.left, rect.top - 20, detection.label.c_str(), detection.label.length());
				}
            }

			// Draw the FPS counter
			SetBkMode(hdc, TRANSPARENT);
			SetTextColor(hdc, RGB(255, 255, 255));
			RECT fpsRect = { 10, 10, 200, 50 };
			std::string fpsText = "FPS: " + std::to_string(fps);
			DrawText(hdc, fpsText.c_str(), -1, &fpsRect, DT_LEFT | DT_TOP | DT_SINGLELINE);

            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_ERASEBKGND: {
            // Prevent flickering by not erasing the background
            return 1;
        }
        case WM_DESTROY: {
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

HWND showResultsWindow(cv::Mat& image, std::vector<DetectionBox>& detections, HMONITOR captureMonitor) {
    HINSTANCE hInstance = GetModuleHandle(NULL);
    const char CLASS_NAME[] = "ResultWindow";

    WNDCLASS wc = {};
    wc.lpfnWndProc = resultWindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);

    RegisterClass(&wc);

	// Default to the capture monitor, if no other monitor is found
	// this will be the target monitor where the window will be displayed
    HMONITOR targetMonitor = captureMonitor;

    // Enumerate monitors and find one that is not the capture monitor
    MONITORINFO monitorInfo = { sizeof(MONITORINFO) };
    EnumDisplayMonitors(NULL, NULL, [](HMONITOR hMonitor, HDC, LPRECT, LPARAM lParam) -> BOOL {
        if (hMonitor != *(HMONITOR*)lParam) {
            *(HMONITOR*)lParam = hMonitor;
            return FALSE; // Stop enumeration
        }
        return TRUE; // Continue enumeration
    }, (LPARAM)&targetMonitor);

	// Fetch monitor rect
    GetMonitorInfo(targetMonitor, &monitorInfo);
    RECT rc = monitorInfo.rcMonitor;

	// Create the window on the target monitor
    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        "Detection Results",
        WS_OVERLAPPEDWINDOW,
        rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
        NULL,
        NULL,
        hInstance,
        NULL
    );

	// Initialize to null to show the initializing text
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)NULL);

	// Set the window to be maximized
    ShowWindow(hwnd, SW_SHOWMAXIMIZED);

	// Update the window to show the initial text,
	// before model load blocks the main thread
    UpdateWindow(hwnd); // Ensure the window is updated
    // RedrawWindow(hwnd, NULL, NULL, RDW_UPDATENOW | RDW_ALLCHILDREN);

    return hwnd;
}