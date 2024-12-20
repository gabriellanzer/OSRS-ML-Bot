#pragma once

// Std dependencies
#include <vector>
#include <string>

class IBotTask
{
public:
	IBotTask() = default;
	virtual ~IBotTask() = default;
	virtual bool Load() = 0;
	virtual void Run(float deltaTime) = 0;
	virtual void Draw() = 0;
	virtual const char* GetName() = 0;
	virtual void GetInputResources(std::vector<std::string>& resources) { };
	virtual void GetOutputResources(std::vector<std::string>& resources) { };
	virtual void GetNextTask(IBotTask*& nextTask) { nextTask = _nextTask; };
	virtual void SetNextTask(IBotTask* nextTask) { _nextTask = nextTask; };

private:
	// Next task in the chain
	IBotTask* _nextTask = nullptr;

};