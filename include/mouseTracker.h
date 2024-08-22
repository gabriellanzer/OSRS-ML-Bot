#pragma once

// Windows Dependencies
#include <windows.h>

// Std Dependencies
#include <thread>

class MouseTracker
{
public:
	MouseTracker();
	~MouseTracker();


	void GetMousePosition(int& x, int& y);
	bool GetMouseDownPosition(int& x, int& y);
	bool GetMouseUpPosition(int& x, int& y);

private:
	void trackMouse();

	bool _mouseDown = false;
	bool _mouseUp = false;
	bool _running = false;

	POINT _mousePosition;
	POINT _mouseDownPosition;
	POINT _mouseUpPosition;

	std::thread _mouseThread;
};