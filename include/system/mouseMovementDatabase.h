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
	void QueryMovement(cv::Point iniPos, cv::Point endPos, float threshold, MouseMovement& outMovements, float minTime = 0.0f, float maxTime = 10.0f);

	std::vector<MouseMovement>& GetMovements() { return _mouseMovements; }

  private:
	MouseMovementDatabase() = default;
	~MouseMovementDatabase() = default;

	MouseMovementDatabase(const MouseMovementDatabase&) = delete;
	MouseMovementDatabase& operator=(const MouseMovementDatabase&) = delete;

	std::vector<MouseMovement> _mouseMovements;

	// Multiple arrays for query parameters per movement
	std::vector<float> _relativeMouseAngles;
	std::vector<float> _relativeMouseDistances;
	std::vector<float> _relativeMouseRandomWeights;
	std::vector<cv::Point> _relativeMouseTargetPoints;
	std::vector<MouseMovement> _relativeMouseMovements;

	// Query state
	std::vector<int> _queryCandidatesIds;
	std::vector<float> _queryCandidatesWeights;
};