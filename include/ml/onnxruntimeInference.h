#pragma once
#include <onnxruntime_cxx_api.h>

#include <opencv2/core.hpp>
#include <opencv2/dnn/dnn.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/videoio.hpp>


// Common functions
class OnnxInferenceBase
{
  public:
	OnnxInferenceBase() = delete;
	OnnxInferenceBase(const std::vector<const char*>& inputNodeNames, const std::vector<const char*>& outputNodeNames,  const std::vector<int64_t> inputNodeDims)
		: _session(Ort::Session(nullptr)), _inputNodeNames(inputNodeNames), _outputNodeNames(outputNodeNames), _inputNodeDims(inputNodeDims)
	{
		// Make sure the environment is initialized
		_env = Ort::Env(OrtLoggingLevel::ORT_LOGGING_LEVEL_WARNING, "Default");
	}
	virtual ~OnnxInferenceBase() = default;

	// Create session
	virtual bool LoadModel(bool useCuda, const wchar_t* modelPath);
	virtual int Inference(cv::Mat& frame, std::vector<Ort::Value>& outputTensor) = 0;

  protected:
	void setSessionOptions(bool useCuda);

	Ort::Env _env;
	Ort::Session _session;
	Ort::SessionOptions _sessionOptions;
	OrtCUDAProviderOptions _cudaOptions;
	Ort::MemoryInfo _memoryInfo{nullptr};

	std::vector<const char*> _inputNodeNames;
	std::vector<const char*> _outputNodeNames;
	std::vector<int64_t> _inputNodeDims;

	cv::Mat _blob;
};

struct YoloDetectionBox
{
    float x, y, w, h;
    int classId;

	bool IsSimilar(const YoloDetectionBox& other, float centerThreshold = 100.0f, float overlapRatioThreshold = 0.25f) const
	{
		// // Check center distance first
		// float cx1 = x + w / 2;
		// float cy1 = y + h / 2;
		// float cx2 = other.x + other.w / 2;
		// float cy2 = other.y + other.h / 2;
		// float dist = std::sqrt(std::pow(cx1 - cx2, 2) + std::pow(cy1 - cy2, 2));
		// if (dist > centerThreshold) return false;

		// Check border distance, against threshold
		float x1 = x;
		float y1 = y;
		float x2 = x + w;
		float y2 = y + h;

		float x3 = other.x;
		float y3 = other.y;
		float x4 = other.x + other.w;
		float y4 = other.y + other.h;

		float dx = std::min(x2, x4) - std::max(x1, x3);
		float dy = std::min(y2, y4) - std::max(y1, y3);

		if (dx <= 0 || dy <= 0) return false;

		float overlapArea = dx * dy;
		float area1 = w * h;
		float area2 = other.w * other.h;
		// TODO: Perhaps just use area1 here, as this function related to the current box
		float overlapRatio = overlapArea / std::min(area1, area2);

		// If there is more overlap than the threshold, consider it similar
		return overlapRatio > overlapRatioThreshold;
	}

	bool Overlaps(const YoloDetectionBox& other) const
	{
		float x1 = x;
		float y1 = y;
		float x2 = x + w;
		float y2 = y + h;

		float x3 = other.x;
		float y3 = other.y;
		float x4 = other.x + other.w;
		float y4 = other.y + other.h;

		if (x1 > x4 || x2 < x3) return false;
		if (y1 > y4 || y2 < y3) return false;

		return true;
	}

	YoloDetectionBox Merge(const YoloDetectionBox& other) const
	{
		assert(classId == other.classId && "Can't merge boxes of different classes!");

		float x1 = std::min(x, other.x);
		float y1 = std::min(y, other.y);
		float x2 = std::max(x + w, other.x + other.w);
		float y2 = std::max(y + h, other.y + other.h);
		return { x1, y1, x2 - x1, y2 - y1, classId };
	}
};

class YOLOInterfaceBase : public OnnxInferenceBase
{
  public:
	YOLOInterfaceBase(const std::vector<const char*>& inputNodeNames, const std::vector<const char*>& outputNodeNames,
					  const std::vector<int64_t> inputNodeDims)
		: OnnxInferenceBase(inputNodeNames, outputNodeNames, inputNodeDims)
	{
	}
	virtual ~YOLOInterfaceBase() = default;

	virtual void Inference(cv::Mat& frame, std::vector<YoloDetectionBox>& detectionBoxes) = 0;

  protected:
	virtual bool preProcess(cv::Mat& frame, std::vector<Ort::Value>& inputTensor) = 0;
	virtual int Inference(cv::Mat& frame, std::vector<Ort::Value>& outputTensor) final;
};

class YOLOv8 : public YOLOInterfaceBase
{
  public:
	YOLOv8(int classNumber, float confidenceThreshold) : YOLOInterfaceBase({ "images" }, { "output0" }, { 1, 3, 640, 640 })
		, _classNumber(classNumber), _confidenceThreshold(confidenceThreshold) {}
	virtual void Inference(cv::Mat& frame, std::vector<YoloDetectionBox>& detectionBoxes) override;

  protected:
	virtual bool preProcess(cv::Mat& frame, std::vector<Ort::Value>& inputTensor) override;

	// Model specific config
	int _classNumber;
	float _confidenceThreshold;

	// Inference state
	std::vector<Ort::Value> _outputTensor;
	cv::Vec2f _outputScaling;
};
