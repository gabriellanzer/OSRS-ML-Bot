#pragma once

// Std dependencies
#include <vector>

// Internal dependencies
#include <system/mouseMovement.h>

class MouseMovementDatabase
{
  public:
	static MouseMovementDatabase& GetInstance();

	void SaveMovements();
	void LoadMovements();
	void UpdateDatabase();
	void QueryMovement(cv::Point iniPos, cv::Point endPos, float threshold, std::vector<MouseMovement>& outMovements);

	std::vector<MouseMovement>& GetMovements() { return _mouseMovements; }

  private:
	MouseMovementDatabase() = default;
	~MouseMovementDatabase() = default;

	MouseMovementDatabase(const MouseMovementDatabase&) = delete;
	MouseMovementDatabase& operator=(const MouseMovementDatabase&) = delete;

	std::vector<MouseMovement> _mouseMovements;

	// Multiple arrays for query parameters per movement
	std::vector<float> _relativeMouseAngles;
	std::vector<MouseMovement> _relativeMouseMovements;
};