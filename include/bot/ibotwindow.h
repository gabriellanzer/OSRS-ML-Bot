#pragma once

class IBotWindow
{
public:
	IBotWindow() = default;
	virtual ~IBotWindow() = default;
	virtual void Run() = 0;
};