#pragma once

// Windows Dependencies
#define NOMINMAX
#include <windows.h>

// Std Dependencies
#include <thread>

// Internal Dependencies
#include <system/mouseMovement.h>

class InputManager
{
public:
	static InputManager& GetInstance()
	{
		static InputManager instance;
		assert(instance._running && "Trying to access InputManager after it has been shutdown!");
		return instance;
	}

	void Initialize();
	void Shutdown();

	void SetPoolingRate(uint32_t rate) { _poolingRate = rate; }

	void GetMousePosition(cv::Point& pos) const;
	bool GetMouseDownPosition(cv::Point& pos, MouseButton button);
	bool GetMouseUpPosition(cv::Point& pos, MouseButton button);

	void SetMousePosition(cv::Point pos, MouseButton button = MOUSE_BUTTON_LEFT, MouseClickState state = MOUSE_CLICK_NONE);

	bool IsEscapePressed() const;
	bool IsTabPressed() const;
	bool IsCapsLockOn() const;
	bool IsShiftPressed() const;
	void SetCapsLock(bool state);

private:
	InputManager();
	~InputManager();

	void trackMouse();

	bool _mouseDown[3] = { false, false, false };
	bool _mouseUp[3] = { false, false, false };

	// Used because we can't reset mouse down
	// as that's the user-facing flag and if
	// the user didn't read it we don't want
	// to lose the information by resetting it
	bool _internalMouseDown[3] = { false, false, false };
	POINT _mousePosition;
	POINT _mouseDownPosition[3];
	POINT _mouseUpPosition[3];

	// Pooling rate (in Hz) at which mouse data is refreshed
	uint32_t _poolingRate = 60;

	bool _running = false;
	std::thread _mouseThread;
};