#pragma once

// Std dependencies
#include <vector>

// Internal dependencies
#include <ml/onnxruntimeInference.h>
#include <bot/ibotTask.h>

enum OreItems
{
	EMPTY = 0,
	CRYSTAL_ORE = 1,
	ADAMANTITE_ORE = 2,
	COPPER_ORE = 3,
	COAL_ORE = 4,
	RUNITE_ORE = 5,
	MITHRIL_ORE = 6,
	SILVER_ORE = 7,
	LOVAKITE_ORE = 8,
	TIN_ORE = 9,
	IRON_ORE = 10,
	GOLD_ORE = 11,
	ELEMENTAL_ORE = 12,
	BLASTED_ORE = 13,
	CORRUPTED_ORE = 14,
	LUNAR_ORE = 15,
	DAEYALT_ORE = 16,
	BLURITE_ORE = 17
};

static const char* OreNames[] = {
	"Empty",
	"Crystal Ore",
	"Adamantite Ore",
	"Copper Ore",
	"Coal Ore",
	"Runite Ore",
	"Mithril Ore",
	"Silver Ore",
	"Lovakite Ore",
	"Tin Ore",
	"Iron Ore",
	"Gold Ore",
	"Elemental Ore",
	"Blasted Ore",
	"Corrupted Ore",
	"Lunar Ore",
	"Daeyalt Ore",
	"Blurite Ore"
};

class InventoryDropTask : public IBotTask
{
public:
	InventoryDropTask();
	virtual ~InventoryDropTask();
	virtual bool Load() override;
	virtual void Run(float deltaTime) override;
	virtual void Draw() override;

	virtual const char* GetName() override { return "Inventory Drop Task"; }
	virtual void GetInputResources(std::vector<std::string>& resources) override;
	virtual void GetOutputResources(std::vector<std::string>& resources) override { };

private:
	// Internal state
	class YOLOv8* _model;
	std::vector<YoloDetectionBox> _detectedItems;

	// Public state
	wchar_t* _modelPath = nullptr;
	float _confidenceThreshold = 0.935f;
};