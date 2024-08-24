#pragma once

struct GLFWwindow;

class IBotWindow
{
public:
	IBotWindow() = delete;
	IBotWindow(GLFWwindow* window) : _nativeWindow(window) {}
	virtual ~IBotWindow() { _nativeWindow = nullptr; };
	virtual void Run(float deltaTime) = 0;

protected:
	GLFWwindow* _nativeWindow = nullptr;
};