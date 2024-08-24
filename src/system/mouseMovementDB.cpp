#include <system/mouseMovementDB.h>

// Std dependencies
#include <fstream>

// Third party dependencies
#include <nlohmann/json.hpp>

// Internal dependencies
// #include <utils.h>

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
		jMovement["clickState"] = static_cast<uint8_t>(movement.clickState);
		jMovement["button"] = static_cast<uint8_t>(movement.button);
		for (const auto& point : movement.points)
		{
			nlohmann::json jPoint;
			jPoint["x"] = point.point.x;
			jPoint["y"] = point.point.y;
			jPoint["deltaTime"] = point.deltaTime;
			jMovement.push_back(jPoint);
		}
		j.push_back(jMovement);
	}

	std::ofstream file("mouse_movements.json");
	file << j.dump(4);
}

void MouseMovementDatabase::LoadMovements()
{
	std::ifstream file("mouse_movements.json");
	nlohmann::json j;
	file >> j;
	file.close();

	_mouseMovements.clear();
	for (const auto& jMovement : j)
	{
		MouseMovement movement;
		movement.color = cv::Scalar(jMovement["color"][0], jMovement["color"][1], jMovement["color"][2], 255);
		movement.clickState = static_cast<MouseClickState>(jMovement["clickState"]);
		movement.button = static_cast<MouseButton>(jMovement["button"]);
		for (const auto& jPoint : jMovement)
		{
			if (jPoint.is_object())
			{
				movement.AddPoint(cv::Point(jPoint["x"], jPoint["y"]), jPoint["deltaTime"]);
			}
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
			movement.points[i].point -= firstPoint.point;
		}
		firstPoint.point = cv::Point(0, 0);
	}
}