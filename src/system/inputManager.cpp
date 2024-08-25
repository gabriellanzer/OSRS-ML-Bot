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

void InputManager::GetMousePosition(cv::Point& pos)
{
	pos.x = _mousePosition.x;
	pos.y = _mousePosition.y;
}

bool InputManager::GetMouseDownPosition(cv::Point& pos)
{
	pos.x = _mouseDownPosition.x;
	pos.y = _mouseDownPosition.y;
	if (_mouseDown)
	{
		_mouseDown = false;
		return true;
	}
	return false;
}

bool InputManager::GetMouseUpPosition(cv::Point& pos)
{
	pos.x = _mouseUpPosition.x;
	pos.y = _mouseUpPosition.y;
	if (_mouseUp)
	{
		_mouseUp = false;
		return true;
	}
	return false;
}

void InputManager::SetMousePosition(cv::Point pos, MouseClickState state)
{
	SetCursorPos(pos.x, pos.y);

	if (state != MOUSE_MOVE)
	{
		INPUT input = {};
		input.type = INPUT_MOUSE;
		input.mi.dx = pos.x;
		input.mi.dy = pos.y;
		input.mi.dwFlags = state == MOUSE_DOWN ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
		SendInput(1, &input, sizeof(INPUT));
	}
}

bool InputManager::IsEscapePressed()
{
	return GetAsyncKeyState(VK_ESCAPE) & 0x8000;
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