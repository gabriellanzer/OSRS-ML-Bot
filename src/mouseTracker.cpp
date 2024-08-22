#include <mouseTracker.h>

#include <iostream>

MouseTracker::MouseTracker()
{
	_running = true;
	_mouseThread = std::thread(&MouseTracker::trackMouse, this);
}

MouseTracker::~MouseTracker()
{
	// Signal the thread to stop
	_running = false;

	// Wait for the thread to finish
	_mouseThread.join();
}

void MouseTracker::GetMousePosition(int& x, int& y)
{
	x = _mousePosition.x;
	y = _mousePosition.y;
}

bool MouseTracker::GetMouseDownPosition(int& x, int& y)
{
	x = _mouseDownPosition.x;
	y = _mouseDownPosition.y;
	if (_mouseDown)
	{
		_mouseDown = false;
		return true;
	}
	return false;
}

bool MouseTracker::GetMouseUpPosition(int& x, int& y)
{
	x = _mouseUpPosition.x;
	y = _mouseUpPosition.y;
	if (_mouseUp)
	{
		_mouseUp = false;
		return true;
	}
	return false;
}

// Global state
HHOOK mouseHook = NULL;
bool mouseDown = false;
bool mouseUp = false;
POINT lastMousePos;
POINT lastMouseDownPos;
POINT lastMouseUpPos;

// Hook procedure to handle mouse events
LRESULT CALLBACK lowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION)
    {
		MSLLHOOKSTRUCT* pMouseStruct = (MSLLHOOKSTRUCT*)lParam;
		if (wParam == WM_LBUTTONDOWN)
		{
			mouseDown = true;
			lastMouseDownPos = pMouseStruct->pt;
		}
		else if (wParam == WM_LBUTTONUP)
		{
			mouseUp = true;
			lastMouseUpPos = pMouseStruct->pt;
		}
		else if (wParam == WM_MOUSEMOVE)
		{
			lastMousePos = pMouseStruct->pt;
		}
	}
    return CallNextHookEx(mouseHook, nCode, wParam, lParam);
}

void MouseTracker::trackMouse()
{
    // Set the mouse hook
    mouseHook = SetWindowsHookEx(WH_MOUSE_LL, lowLevelMouseProc, NULL, 0);
    if (!mouseHook)
    {
        std::cout << "Failed to set mouse hook\n";
        return;
    }

    // Message loop to keep the hook alive
    MSG msg;
    while (_running)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

		// Update mouse state
		if (mouseDown)
		{
			_mouseDown = true;
			_mouseDownPosition = lastMouseDownPos;
			mouseDown = false;
		}
		if (mouseUp)
		{
			_mouseUp = true;
			_mouseUpPosition = lastMouseUpPos;
			mouseUp = false;
		}
		_mousePosition = lastMousePos;
    }

    // Unhook the mouse hook when done
    UnhookWindowsHookEx(mouseHook);
}