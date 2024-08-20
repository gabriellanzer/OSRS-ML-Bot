#pragma once
#include <onnxruntime_cxx_api.h>

#include <opencv2/core.hpp>
#include <opencv2/dnn/dnn.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/videoio.hpp>


// This have to be the first thing called
struct OnnxENV
{
	Ort::Env env = Ort::Env(OrtLoggingLevel::ORT_LOGGING_LEVEL_WARNING, "Default");
};

// Common functions
class OnnxInferenceBase
{
  public:
	// Settings
	void SetSessionOptions(bool useCuda);
	void SetUseCuda();

	// Create session
	bool LoadWeights(OnnxENV* onnxEnv, const wchar_t* modelPath);
	void SetInputNodeNames(std::vector<const char*>* names);
	void SetOutputNodeNames(std::vector<const char*>* names);
	void SetInputDemensions(std::vector<int64_t> dims);
	virtual int Inference(cv::Mat& frame, std::vector<Ort::Value>& outputTensor) = 0;

  protected:
	Ort::Session _session = Ort::Session(nullptr);
	Ort::SessionOptions _sessionOptions;
	OrtCUDAProviderOptions _cudaOptions;
	Ort::MemoryInfo _memoryInfo{nullptr};				  // Used to allocate memory for input
	std::vector<const char*>* _outputNodeNames = nullptr; // output node names
	std::vector<const char*>* _inputNodeNames = nullptr;  // Input node names
	std::vector<int64_t> _inputNodeDims;				  // Input node dimension
	cv::Mat _blob;										  // Converted input. In this case for the (1,3,640,640)
};

// model specifics
class YOLOv10 : public OnnxInferenceBase
{
  public:
	virtual int Inference(cv::Mat& frame, std::vector<Ort::Value>& outputTensor) override;

  protected:
	bool PreProcess(cv::Mat& frame, std::vector<Ort::Value>& inputTensor);
};

class YOLOv8_OBB : public OnnxInferenceBase
{
  public:
	virtual int Inference(cv::Mat& frame, std::vector<Ort::Value>& outputTensor) override;

  protected:
	bool PreProcess(cv::Mat& frame, std::vector<Ort::Value>& inputTensor);
};

class YOLOv8 : public OnnxInferenceBase
{
  public:
	virtual int Inference(cv::Mat& Frame, std::vector<Ort::Value>& outputTensor) override;

  protected:
	bool PreProcess(cv::Mat& frame, std::vector<Ort::Value>& inputTensor);
};