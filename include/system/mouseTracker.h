#pragma once

// Windows Dependencies
#include <windows.h>

// Std Dependencies
#include <thread>

class MouseTracker
{
public:
	// Pooling rate (in Hz) at which mouse data is refreshed
	MouseTracker(uint32_t poolingRate = 60);
	~MouseTracker();


	void GetMousePosition(int& x, int& y);
	bool GetMouseDownPosition(int& x, int& y);
	bool GetMouseUpPosition(int& x, int& y);

private:
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

	uint32_t _poolingRate = 60;

	std::thread _mouseThread;
};