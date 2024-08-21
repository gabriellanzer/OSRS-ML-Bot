#include <windowPicker.h>

// Std Dependencies
#include <string>
#include <vector>

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

std::pair<HDC, HMONITOR> pickMonitorDialog() {
    std::vector<MonitorInfo> monitors;
    EnumDisplayMonitors(NULL, NULL, monitorEnumFunc, reinterpret_cast<LPARAM>(&monitors));

    if (monitors.empty()) {
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