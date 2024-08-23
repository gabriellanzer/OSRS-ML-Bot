#include <system/resultsWindow.h>

#include <chrono>

struct ResultData
{
    cv::Mat& image;
    std::vector<YoloDetectionBox>& detections;
};

double fps = 0.0;
void sampleFramerate()
{
	static auto lastTime = std::chrono::high_resolution_clock::now();
	static int frameCount = 0;
	frameCount++;

	auto currentTime = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastTime);
	if (duration.count() >= 1)
	{
		fps = frameCount / (double)duration.count();
		frameCount = 0;
		lastTime = currentTime;
	}
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
			else if (resultData && !resultData->image.empty())
			{
                BITMAPINFO bmi = {0};
                bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
                bmi.bmiHeader.biWidth = resultData->image.cols;
                bmi.bmiHeader.biHeight = -resultData->image.rows; // Negative to indicate top-down bitmap
                bmi.bmiHeader.biPlanes = 1;
                bmi.bmiHeader.biBitCount = 24;
                bmi.bmiHeader.biCompression = BI_RGB;

                StretchDIBits(hdc, 0, 0, resultData->image.cols, resultData->image.rows, 0, 0, resultData->image.cols, resultData->image.rows, resultData->image.data, &bmi, DIB_RGB_COLORS, SRCCOPY);

                for (const auto& detection : resultData->detections) {
					std::string label;
					cv::Scalar color = cv::Scalar(0, 0, 0);
					if (detection.classId == 0) { color = cv::Scalar(0, 128, 0);		label = "Adamant";	};
					if (detection.classId == 1) { color = cv::Scalar(79, 69, 54);		label = "Coal";		};
					if (detection.classId == 2) { color = cv::Scalar(51, 115, 184); 	label = "Copper";	};
					if (detection.classId == 3) { color = cv::Scalar(34, 34, 178);		label = "Iron";		};
					if (detection.classId == 4) { color = cv::Scalar(180, 130, 70); 	label = "Mithril";	};
					if (detection.classId == 5) { color = cv::Scalar(192, 192, 192);	label = "Silver";	};
					if (detection.classId == 6) { color = cv::Scalar(193, 205, 205);	label = "Tin";		};
					if (detection.classId == 7) { color = cv::Scalar(0, 0, 0);			label = "Wasted";	};

                    RECT rect = { (LONG)detection.x, (LONG)detection.y, (LONG)(detection.x + detection.w), (LONG)(detection.y + detection.h) };
                    HBRUSH brush = CreateSolidBrush(RGB(color[2], color[1], color[0]));
                    FrameRect(hdc, &rect, brush);
                    DeleteObject(brush);

					// Draw the label
					SetBkMode(hdc, TRANSPARENT);
					SetTextColor(hdc, RGB(color[2], color[1], color[0]));
					TextOut(hdc, rect.left, rect.top - 20, label.c_str(), label.length());
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

ResultsWindow::ResultsWindow(HMONITOR captureMonitor) {
	HINSTANCE hInstance = GetModuleHandle(NULL);
    const char CLASS_NAME[] = "OSRS-Bot (Results Window)";

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
    _hwnd = CreateWindowEx(
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
    SetWindowLongPtr(_hwnd, GWLP_USERDATA, (LONG_PTR)NULL);

	// Set the window to be maximized
    ShowWindow(_hwnd, SW_SHOWMAXIMIZED);

	// Update the window to show the initial text,
	// before model load blocks the main thread
    UpdateWindow(_hwnd); // Ensure the window is updated
    // RedrawWindow(hwnd, NULL, NULL, RDW_UPDATENOW | RDW_ALLCHILDREN);
}

ResultsWindow::~ResultsWindow() {
	if (_hwnd != nullptr)
	{
		DestroyWindow(_hwnd);
	}
	_monitor = nullptr;
}

void ResultsWindow::Update(cv::Mat& frame, std::vector<YoloDetectionBox>& detectionBoxes)
{
	// Compute frames per second
	sampleFramerate();

	// This also has to be set every frame
	ResultData resultData = { frame, detectionBoxes };
	SetWindowLongPtr(_hwnd, GWLP_USERDATA, (LONG_PTR)&resultData);

	// Update window content
	InvalidateRect(_hwnd, NULL, TRUE);

	// Process window messages
	MSG msg = {};
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	// Break loop if the window was closed
	if (msg.message == WM_QUIT)
	{
		DestroyWindow(_hwnd);
		_hwnd = nullptr;
	}
}