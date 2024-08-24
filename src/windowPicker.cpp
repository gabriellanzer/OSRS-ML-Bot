#include <system\windowPicker.h>

// Std Dependencies
#include <string>
#include <vector>

struct CustomWndProcData
{
	std::string deviceName;
	WNDPROC originalWndProc;
    int width, height;
	bool hovered;
};

struct MonitorPickInfo {
    GLFWmonitor* monitor;
	GLFWwindow* window;
	HWND hwnd;
	CustomWndProcData* procData;
};

LRESULT CALLBACK customWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

static GLFWwindow* clickedWindow = NULL;

void mouseClickCallback(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
	{
        clickedWindow = window;
    }
}

std::pair<HDC, GLFWmonitor*> pickMonitorDialog()
{
	int monitorCount = 0;
	GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
    if (monitorCount == 0) {
        return { NULL, NULL };
    }

	// Just the primary monitor, no need to pick
	if (monitorCount == 1)
	{
		// Create HDC for the tracked monitor
		const char* adapterName = glfwGetWin32Adapter(monitors[0]);
		HDC hdc = CreateDC(adapterName, NULL, NULL, NULL);
		return { hdc, monitors[0] };
	}

	// Reset selection state
	clickedWindow = NULL;

	// Create a window for each monitor (for picking)
    std::vector<MonitorPickInfo> monitorInfos;
	for (int i = 0; i < monitorCount; ++i)
	{
		MonitorPickInfo monitorInfo;
		monitorInfo.monitor = monitors[i];
		int xpos, ypos, width, height;
		const GLFWvidmode* mode = glfwGetVideoMode(monitorInfo.monitor);
		width = mode->width; height = mode->height;
		glfwGetMonitorPos(monitorInfo.monitor, &xpos, &ypos);

		// Set window creation hints
		glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

		// Create window such as it covers the entire monitor
		monitorInfo.window = glfwCreateWindow(width, height, "", NULL, NULL);
		monitorInfo.hwnd = glfwGetWin32Window(monitorInfo.window);
		glfwSetWindowPos(monitorInfo.window, xpos, ypos);
		glfwMakeContextCurrent(monitorInfo.window);

        // Set the mouse button callback
        glfwSetMouseButtonCallback(monitorInfo.window, mouseClickCallback);

		// Set style and draw the monitor name only once (no more swap buffers after this)
		glfwSwapBuffers(monitorInfo.window);
        LONG exStyle = GetWindowLong(monitorInfo.hwnd, GWL_EXSTYLE);
        SetWindowLong(monitorInfo.hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
        SetLayeredWindowAttributes(monitorInfo.hwnd, 0, 180, LWA_ALPHA);

		// Set proxy window proc function
		CustomWndProcData* windowProcData = new CustomWndProcData();
		const char* adapterName = glfwGetWin32Adapter(monitorInfo.monitor);
		for (const char* p = adapterName; *p; ++p) { if (*p == '\\' && *(p+1)) { adapterName = (p+1); } }
		windowProcData->deviceName = std::string(adapterName);
		windowProcData->width = width;
		windowProcData->height = height;
		windowProcData->hovered = false;
		windowProcData->originalWndProc = (WNDPROC)GetWindowLongPtr(monitorInfo.hwnd, GWLP_WNDPROC);
		monitorInfo.procData = windowProcData;
		SetWindowLongPtr(monitorInfo.hwnd, GWLP_USERDATA, (LONG_PTR)windowProcData);
    	SetWindowLongPtr(monitorInfo.hwnd, GWLP_WNDPROC, (LONG_PTR)customWndProc);

		monitorInfos.push_back(monitorInfo);
	}

	GLFWmonitor* monitorToTrack = glfwGetPrimaryMonitor();
	bool shouldClose = false;
	while (!shouldClose)
	{
		for (auto& monitorInfo : monitorInfos)
		{
			glfwMakeContextCurrent(monitorInfo.window);

			// Request a repaint of the window
			InvalidateRect(monitorInfo.hwnd, NULL, TRUE);

            // Get cursor position
            double cursorX, cursorY;
            glfwGetCursorPos(glfwGetCurrentContext(), &cursorX, &cursorY);

            // Get window position and size
            int windowWidth, windowHeight;
            glfwGetWindowSize(monitorInfo.window, &windowWidth, &windowHeight);

            // Check if cursor is within window bounds
            bool isHovering = cursorX >= 0 && cursorX <= windowWidth &&
                              cursorY >= 0 && cursorY <= windowHeight;

			// Change transparency according to hover state
        	SetLayeredWindowAttributes(monitorInfo.hwnd, 0, isHovering ? 180 : 80, LWA_ALPHA);

            // Update hover state
            monitorInfo.procData->hovered = isHovering;

			// User clicked on a window
			if (clickedWindow == monitorInfo.window)
			{
				monitorToTrack = monitorInfo.monitor;
				shouldClose = true;
				break;
			}

			// If this happens the user closed the window (cancel the operation)
			if (glfwWindowShouldClose(monitorInfo.window))
			{
				return { NULL, NULL };
			}
		}

		glfwPollEvents();
	}

	// Destroy the windows used for picking
	for (auto& monitorInfo : monitorInfos)
	{
		glfwDestroyWindow(monitorInfo.window);
		delete monitorInfo.procData;
	}

	// Create HDC for the tracked monitor
	const char* adapterName = glfwGetWin32Adapter(monitorToTrack);
	HDC hdc = CreateDC(adapterName, NULL, NULL, NULL);
    return { hdc, monitorToTrack };
}

LRESULT CALLBACK customWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CustomWndProcData* windowProcData = reinterpret_cast<CustomWndProcData*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    switch (uMsg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            // Create a memory device context for double buffering
            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBitmap = CreateCompatibleBitmap(hdc, windowProcData->width, windowProcData->height);
            HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

            // Clear the background
            RECT rect = {0, 0, windowProcData->width, windowProcData->height};
            HBRUSH brush = CreateSolidBrush(windowProcData->hovered ? RGB(20, 20, 20) : RGB(0, 0, 0));
            FillRect(memDC, &rect, brush);
            DeleteObject(brush);

            // Draw the text
            SetTextColor(memDC, RGB(255, 255, 255));
            SetBkMode(memDC, TRANSPARENT);
            std::string label = windowProcData->deviceName;
            if (windowProcData->hovered) { label += "\n(Click to select)"; }

            // Calculate the height of the text
            RECT textRect = rect;
            DrawText(memDC, label.c_str(), -1, &textRect, DT_CENTER | DT_WORDBREAK | DT_CALCRECT);

            // Adjust the rectangle to center the text vertically
            int textHeight = textRect.bottom - textRect.top;
            rect.top = (windowProcData->height - textHeight) / 2;
            rect.bottom = rect.top + textHeight;

            // Draw the text in the adjusted rectangle
            DrawText(memDC, label.c_str(), -1, &rect, DT_CENTER | DT_WORDBREAK);


            // Copy the memory device context to the window device context
            BitBlt(hdc, 0, 0, windowProcData->width, windowProcData->height, memDC, 0, 0, SRCCOPY);

            // Clean up
            SelectObject(memDC, oldBitmap);
            DeleteObject(memBitmap);
            DeleteDC(memDC);

            EndPaint(hwnd, &ps);
            break;
        }
		case WM_ERASEBKGND: {
            return 1; // Indicate that the background has been erased
        }
        default: {
            return CallWindowProc(windowProcData->originalWndProc, hwnd, uMsg, wParam, lParam);
        }
    }
    return 0;
}