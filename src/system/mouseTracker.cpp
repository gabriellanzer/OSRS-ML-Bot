#include <system/mouseTracker.h>

MouseTracker::MouseTracker(uint32_t poolingRate)
	: _poolingRate(poolingRate)
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

void MouseTracker::trackMouse()
{
    while (_running)
    {
        // Get the current mouse position
        POINT currentPos;
        GetCursorPos(&currentPos);

        // Check if the left mouse button is down
        if (GetAsyncKeyState(VK_LBUTTON) & 0x8000)
        {
            if (!_internalMouseDown)
            {
				_internalMouseDown = true;
                _mouseDown = true;
                _mouseDownPosition = currentPos;
            }
        }
        else // The left mouse button is not down
        {
			// Check if just released
            if (_internalMouseDown)
            {
                _internalMouseDown = false;
                _mouseUp = true;
                _mouseUpPosition = currentPos;
            }
        }

        // Update the mouse position
        _mousePosition = currentPos;

        // Sleep for a short duration to reduce CPU usage (~60Hz pooling)
        std::this_thread::sleep_for(std::chrono::milliseconds(1000 / _poolingRate));
    }
}