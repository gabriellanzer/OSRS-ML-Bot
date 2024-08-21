
// Windows Dependencies
#include <windows.h>

// Third Party Dependencies
#ifndef GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#endif
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

// Std Dependencies
#include <utility>

std::pair<HDC, GLFWmonitor*> pickMonitorDialog();