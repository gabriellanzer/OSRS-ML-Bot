#pragma once

// Third party dependencies
#include <opencv2/core.hpp>

// Internal dependencies
#include <ml/onnxruntimeInference.h>
#include <bot/ibotTask.h>

enum TabClasses
{
	TAB_ATTACK_STYLE = 0,
	TAB_FRIENDS_LIST = 1,
	TAB_INVENTORY = 2,
	TAB_MAGIC = 3,
	TAB_PRAYER = 4,
	TAB_QUESTS = 5,
	TAB_SKILLS = 6,
	TAB_EQUIPMENTS = 7
};

static const char* TabNames[] = { "Attack Style Tab", "Friends List Tab", "Inventory Tab", "Magic Tab", "Prayer Tab", "Quests Tab", "Skills Tab", "Equipments Tab" };

class FindTabTask : public IBotTask
{
public:
	FindTabTask();
	virtual ~FindTabTask();
	virtual bool Load() override;
	virtual void Run(float deltaTime) override;
	virtual void Draw() override;

	virtual const char* GetName() override { return "Find Tab Task"; }
	virtual void GetInputResources(std::vector<std::string>& resources) override { }
	virtual void GetOutputResources(std::vector<std::string>& resources) override;

	void SetTrackingTab(TabClasses trackingTab) { _trackingTab = trackingTab; }
	cv::Mat GetTabFrame() { return _tabFrame.clone(); }

private:
	// Internal state
	class PreProcessBoxDetectionBase* _model;
	std::vector<DetectionBox> _detectedTabs;
	bool _exportDetection = false;
	bool _shouldOverrideClass = false;
	TabClasses _overrideClass = TAB_INVENTORY;

	// Public state
	wchar_t* _modelPath = nullptr;
	float _confidenceThreshold = 0.935f;
	TabClasses _trackingTab = TAB_INVENTORY;
	cv::Mat _tabFrame;
};