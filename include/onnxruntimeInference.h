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
};

class YOLOInterfaceBase : public OnnxInferenceBase
{
	public:
	  YOLOInterfaceBase(const std::vector<const char*>& inputNodeNames, const std::vector<const char*>& outputNodeNames, const std::vector<int64_t> inputNodeDims)
		  : OnnxInferenceBase(inputNodeNames, outputNodeNames, inputNodeDims) {}

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
