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

	void GetMousePosition(cv::Point& pos);
	bool GetMouseDownPosition(cv::Point& pos);
	bool GetMouseUpPosition(cv::Point& pos);

	void SetMousePosition(cv::Point pos, MouseClickState state = MOUSE_MOVE);

	bool IsEscapePressed();

private:
	InputManager();
	~InputManager();

	void trackMouse();

	bool _mouseDown = false;
	bool _mouseUp = false;
	bool _running = false;

	// Used because we can't reset mouse down
	// as that's the user-facing flag and if
	// the user didn't read it we don't want
	// to lose the information by resetting it
	bool _internalMouseDown = false;
	POINT _mousePosition;
	POINT _mouseDownPosition;
	POINT _mouseUpPosition;

	// Pooling rate (in Hz) at which mouse data is refreshed
	uint32_t _poolingRate = 60;

	std::thread _mouseThread;
};