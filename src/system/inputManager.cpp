#include <system/inputManager.h>

// Windows Dependencies
#include <windows.h>

InputManager::InputManager() : _poolingRate(60) { Initialize(); }

InputManager::~InputManager() { Shutdown(); }

void InputManager::Initialize()
{
	_running = true;
	_mouseThread = std::thread(&InputManager::trackMouse, this);
}

void InputManager::Shutdown()
{
	// Signal the thread to stop
	_running = false;

	// Wait for the thread to finish
	_mouseThread.join();
}

void InputManager::GetMousePosition(cv::Point& pos) const
{
	pos.x = _mousePosition.x;
	pos.y = _mousePosition.y;
}

bool InputManager::GetMouseDownPosition(cv::Point& pos, MouseButton button)
{
	pos.x = _mouseDownPosition[button].x;
	pos.y = _mouseDownPosition[button].y;
	if (_mouseDown[button])
	{
		_mouseDown[button] = false;
		return true;
	}
	return false;
}

bool InputManager::GetMouseUpPosition(cv::Point& pos, MouseButton button)
{
	pos.x = _mouseUpPosition[button].x;
	pos.y = _mouseUpPosition[button].y;
	if (_mouseUp[button])
	{
		_mouseUp[button] = false;
		return true;
	}
	return false;
}

void InputManager::SetMousePosition(cv::Point pos, MouseButton button, MouseClickState state)
{
	SetCursorPos(pos.x, pos.y);

	if (state != MOUSE_CLICK_NONE)
	{
		INPUT input = {};
		input.type = INPUT_MOUSE;
		input.mi.dx = pos.x;
		input.mi.dy = pos.y;

		if (button == MOUSE_BUTTON_LEFT)
		{
			input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
		}
		else if (button == MOUSE_BUTTON_RIGHT)
		{
			input.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
		}
		else if (button == MOUSE_BUTTON_MIDDLE)
		{
			input.mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
		}
		if (state == MOUSE_CLICK_UP)
		{
			// Up flags are double the value of down flags
			input.mi.dwFlags *= 2;
		}
		SendInput(1, &input, sizeof(INPUT));
	}
}

bool InputManager::IsEscapePressed() const
{
	return GetAsyncKeyState(VK_ESCAPE) & 0x8000;
}

bool InputManager::IsTabPressed() const
{
	return GetAsyncKeyState(VK_TAB) & 0x8000;
}

bool InputManager::IsCapsLockOn() const
{
	return GetKeyState(VK_CAPITAL) & 1;
}

void InputManager::trackMouse()
{
	while (_running)
	{
		// Get the current mouse position
		POINT currentPos;
		GetCursorPos(&currentPos);

		// Check if the left mouse button is down
		if (GetAsyncKeyState(VK_LBUTTON) & 0x8000)
		{
			if (!_internalMouseDown[MOUSE_BUTTON_LEFT])
			{
				_internalMouseDown[MOUSE_BUTTON_LEFT] = true;
				_mouseDown[MOUSE_BUTTON_LEFT] = true;
				_mouseDownPosition[MOUSE_BUTTON_LEFT] = currentPos;
			}
		}
		else // The left mouse button is not down
		{
			// Check if just released
			if (_internalMouseDown[MOUSE_BUTTON_LEFT])
			{
				_internalMouseDown[MOUSE_BUTTON_LEFT] = false;
				_mouseUp[MOUSE_BUTTON_LEFT] = true;
				_mouseUpPosition[MOUSE_BUTTON_LEFT] = currentPos;
			}
		}

		// Check if the right mouse button is down
		if (GetAsyncKeyState(VK_RBUTTON) & 0x8000)
		{
			if (!_internalMouseDown[MOUSE_BUTTON_RIGHT])
			{
				_internalMouseDown[MOUSE_BUTTON_RIGHT] = true;
				_mouseDown[MOUSE_BUTTON_RIGHT] = true;
				_mouseDownPosition[MOUSE_BUTTON_RIGHT] = currentPos;
			}
		}
		else // The right mouse button is not down
		{
			// Check if just released
			if (_internalMouseDown[MOUSE_BUTTON_RIGHT])
			{
				_internalMouseDown[MOUSE_BUTTON_RIGHT] = false;
				_mouseUp[MOUSE_BUTTON_RIGHT] = true;
				_mouseUpPosition[MOUSE_BUTTON_RIGHT] = currentPos;
			}
		}

		// Check if the middle mouse button is down
		if (GetAsyncKeyState(VK_MBUTTON) & 0x8000)
		{
			if (!_internalMouseDown[MOUSE_BUTTON_MIDDLE])
			{
				_internalMouseDown[MOUSE_BUTTON_MIDDLE] = true;
				_mouseDown[MOUSE_BUTTON_MIDDLE] = true;
				_mouseDownPosition[MOUSE_BUTTON_MIDDLE] = currentPos;
			}
		}
		else // The middle mouse button is not down
		{
			// Check if just released
			if (_internalMouseDown[MOUSE_BUTTON_MIDDLE])
			{
				_internalMouseDown[MOUSE_BUTTON_MIDDLE] = false;
				_mouseUp[MOUSE_BUTTON_MIDDLE] = true;
				_mouseUpPosition[MOUSE_BUTTON_MIDDLE] = currentPos;
			}
		}

		// Update the mouse position
		_mousePosition = currentPos;

		// Sleep for a short duration to reduce CPU usage (~60Hz pooling)
		std::this_thread::sleep_for(std::chrono::milliseconds(1000 / _poolingRate));
	}
}