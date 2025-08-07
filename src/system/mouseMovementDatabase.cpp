#include <system/mouseMovementDatabase.h>

// Std dependencies
#include <fstream>
#include <iostream>

// Third party dependencies
#include <nlohmann/json.hpp>

// Internal dependencies
#include <utils.h>

MouseMovementDatabase& MouseMovementDatabase::GetInstance()
{
	static MouseMovementDatabase instance;
	return instance;
}

void MouseMovementDatabase::SaveMovements()
{
	nlohmann::json j;
	for (const auto& movement : _mouseMovements)
	{
		nlohmann::json jMovement;
		jMovement["color"] = { movement.color[0], movement.color[1], movement.color[2] };
		jMovement["points"] = nlohmann::json::array();
		for (const auto& point : movement.points)
		{
			nlohmann::json jPoint;
			jPoint["x"] = point.pos.x;
			jPoint["y"] = point.pos.y;
			jPoint["deltaTime"] = point.deltaTime;
			jMovement["points"].push_back(jPoint);
		}
		j.push_back(jMovement);
	}

	std::ofstream file("mouse_movements.json");
	file << j.dump(4);
}

void MouseMovementDatabase::LoadMovements()
{
	std::ifstream file("mouse_movements.json");
	if (!file.is_open())
	{
		std::cout << "Mouse movements file not found, starting with empty database." << std::endl;
		_mouseMovements.clear();
		_mouseMovementsLoaded = true;
		return;
	}

	nlohmann::json j;
	try
	{
		file >> j;
	}
	catch (const std::exception& ex)
	{
		std::cout << "Mouse movements load error! Msg: " << ex.what() << std::endl;
		return;
	}
	file.close();

	_mouseMovements.clear();
	for (const auto& jMovement : j)
	{
		MouseMovement movement;
		auto& color = jMovement["color"];
		movement.color = cv::Scalar(color[0], color[1], color[2], 255);
		for (const auto& jPoint : jMovement["points"])
		{
			movement.AddPoint(cv::Point(jPoint["x"], jPoint["y"]), jPoint["deltaTime"]);
		}
		_mouseMovements.push_back(movement);
	}
}

void MouseMovementDatabase::UpdateDatabase()
{
	_relativeMouseMovements = _mouseMovements;
	for (auto& movement : _relativeMouseMovements)
	{
		if (movement.points.empty()) continue;

		MousePoint& firstPoint = movement.points[0];
		for (size_t i = 1; i < movement.points.size(); i++)
		{
			movement.points[i].pos -= firstPoint.pos;
		}
		firstPoint.pos = cv::Point(0, 0);
	}

	// Compute query parameters and sort by them
	size_t movementCount = _relativeMouseMovements.size();
	_relativeMouseAngles.resize(movementCount);
	_relativeMouseDistances.resize(movementCount);
	_relativeMouseTargetPoints.resize(movementCount);
	_relativeMouseRandomWeights.resize(movementCount);

	for (size_t i = 0; i < movementCount; i++)
	{
		const MouseMovement& movement = _relativeMouseMovements[i];
		if (movement.points.empty()) continue;

		// Compute angle
		const cv::Point& lastPoint = movement.points.back().pos;
		_relativeMouseAngles[i] = atan2(lastPoint.y, lastPoint.x);

		// Compute distance
		_relativeMouseDistances[i] = cv::norm(lastPoint);

		// Store target point
		_relativeMouseTargetPoints[i] = lastPoint;

		// Start with default weight
		_relativeMouseRandomWeights[i] = 1.0f;
	}
}

void MouseMovementDatabase::QueryMovement(cv::Point iniPos, cv::Point endPos, float threshold, MouseMovement& outMovement, float minTime, float maxTime)
{
	// Compute query parameters
	cv::Point diff = endPos - iniPos;
	float angle = atan2(diff.y, diff.x);
	float distance = cv::norm(diff);

	// Populate with default ids
	_queryCandidatesIds.resize(_relativeMouseMovements.size());
	for (size_t i = 0; i < _relativeMouseMovements.size(); i++)
	{
		_queryCandidatesIds[i] = i;
	}

	// Sort indices by target point distance, min and max time
	std::sort(_queryCandidatesIds.begin(), _queryCandidatesIds.end(), [&](const int a, const int b)
	{
		float totalTimeA = _relativeMouseMovements[a].GetTotalTime();
		float totalTimeB = _relativeMouseMovements[b].GetTotalTime();

		// Sort by min and max time
		if (totalTimeA < minTime && totalTimeB >= minTime) return true;
		if (totalTimeA >= minTime && totalTimeB < minTime) return false;
		if (totalTimeA > maxTime && totalTimeB <= maxTime) return false;
		if (totalTimeA <= maxTime && totalTimeB > maxTime) return true;
		if (totalTimeA < minTime && totalTimeB < minTime) return totalTimeA > totalTimeB;
		if (totalTimeA > maxTime && totalTimeB > maxTime) return totalTimeA < totalTimeB;

		// If both match time contraints, sort by distance
		cv::Point targetDiffA = _relativeMouseTargetPoints[a] - diff;
		cv::Point targetDiffB = _relativeMouseTargetPoints[b] - diff;
		float sqrdDistA = targetDiffA.dot(targetDiffA);
		float sqrdDistB = targetDiffB.dot(targetDiffB);
		return sqrdDistA < sqrdDistB;
	});

	// Count number of matches
	int numMatches = 0;
	int numRelaxedMatches = 0;
	for (int candidateId : _queryCandidatesIds)
	{
		float totalTime = _relativeMouseMovements[candidateId].GetTotalTime();
		float diffDist = cv::norm(_relativeMouseTargetPoints[candidateId] - diff);
		bool matchTime = totalTime >= minTime && totalTime <= maxTime;
		if (diffDist < threshold && matchTime)
		{
			++numMatches;
			++numRelaxedMatches;
		}
		else if (diffDist < threshold && !matchTime)
		{
			++numRelaxedMatches;
		}
		else
		{
			// Early exit because array is sorted
			break;
		}
	}

	// If there are no candidates, return an empty movement for now
	// Later, we can reshape existing movements to match the query
	if (numRelaxedMatches == 0)
	{
		outMovement = MouseMovement();
		return;
	}

	// Resize down to remove non-matching candidates (unless there are only soft-matches)
	if (numMatches == 0) numMatches = numRelaxedMatches;
	_queryCandidatesIds.resize(numMatches);

	// Sort by angle
	std::sort(_queryCandidatesIds.begin(), _queryCandidatesIds.end(), [&](const int a, const int b)
	{
		return std::abs(_relativeMouseMovements[a].GetAngle() - angle) < std::abs(_relativeMouseMovements[b].GetAngle() - angle);
	});

	// Create a random query weight that goes from 0.0f to the sum of harmonic series up to numMatches
	float harmonicSum = 0.0f;
	for (int i = 1; i <= numMatches; i++)
	{
		harmonicSum += 1.0f / static_cast<float>(i);
	}
	float randomWeight = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * harmonicSum;

	int candidatePick = numMatches - 1;
	// Consume our weight by the harmonic weight of each candidate weighted by its random weight
	for (int i = 0; i < numMatches; i++)
	{
		const float harmonicWeight = 1.0f / static_cast<float>(i + 1);
		randomWeight -= harmonicWeight * _relativeMouseRandomWeights[_queryCandidatesIds[i]];
		if (randomWeight < 0.0f)
		{
			candidatePick = i;
			break;
		}
	}

	// We found our candidate
	outMovement = _relativeMouseMovements[_queryCandidatesIds[candidatePick]];

	// Apply the relative position position to the initial position
	for (auto& point : outMovement.points)
	{
		point.pos += iniPos;
	}

	// Decrease the random weight by half the harmonic weight so it's
	// less likely to be selected again for similar query parameters
	float& movementRandWeight = _relativeMouseRandomWeights[_queryCandidatesIds[candidatePick]];
	movementRandWeight = std::max(0.05f, movementRandWeight - (0.5f / static_cast<float>(candidatePick + 1)));

	// Increase the random weight of the other candidates by half their harmonic weights
	for (int i = 0; i < numMatches; i++)
	{
		if (i == candidatePick) continue;
		const float halfHarmonicWeight = 0.5f / static_cast<float>(i + 1);
		_relativeMouseRandomWeights[_queryCandidatesIds[i]] += halfHarmonicWeight;
	}
}
