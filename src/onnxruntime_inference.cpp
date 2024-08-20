#include <onnxruntime_inference.h>

#include <opencv2/imgproc.hpp>

#include <iostream>

void OnnxInferenceBase::SetSessionOptions(bool useCuda)
{
	_sessionOptions.SetInterOpNumThreads(1);
	_sessionOptions.SetIntraOpNumThreads(1);
	// Optimization will take time and memory during startup
	_sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_DISABLE_ALL);
	// CUDA options. If used.
	if (useCuda) { SetUseCuda(); }
}
void OnnxInferenceBase::SetUseCuda()
{
	_cudaOptions.device_id = 0;												// GPU_ID
	_cudaOptions.cudnn_conv_algo_search = OrtCudnnConvAlgoSearchExhaustive; // Algo to search for Cudnn
	_cudaOptions.arena_extend_strategy = 0;
	// May cause data race in some condition
	_cudaOptions.do_copy_in_default_stream = 0;
	_sessionOptions.AppendExecutionProvider_CUDA(_cudaOptions); // Add CUDA options to session options
}

bool OnnxInferenceBase::LoadWeights(OnnxENV* onnxEnv, const wchar_t* modelPath)
{
	try
	{
		// Model path is const wchar_t*
		_session = Ort::Session(onnxEnv->env, modelPath, _sessionOptions);
	}
	catch (Ort::Exception oe)
	{
		std::cout << "ONNX exception caught: " << oe.what() << ", Code: " << oe.GetOrtErrorCode() << ".\n";
		return false;
	}
	try
	{ // For allocating memory for input tensors
		_memoryInfo = std::move(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault));
	}
	catch (Ort::Exception oe)
	{
		std::cout << "ONNX exception caught: " << oe.what() << ", Code: " << oe.GetOrtErrorCode() << ".\n";
		return false;
	}
	return true;
}

void OnnxInferenceBase::SetInputNodeNames(std::vector<const char*>* names) { _inputNodeNames = names; }

void OnnxInferenceBase::SetOutputNodeNames(std::vector<const char*>* names) { _outputNodeNames = names; }

void OnnxInferenceBase::SetInputDemensions(std::vector<int64_t> dims) { _inputNodeDims = dims; }

bool YOLOv10::PreProcess(cv::Mat& image, std::vector<Ort::Value>& inputTensor)
{
	// this will make the input into 1,3,640,640
	_blob = cv::dnn::blobFromImage(image, 1 / 255.0, cv::Size(640, 640), cv::Scalar(0, 0, 0), false, false);
	size_t inputTensorSize = _blob.total();
	try
	{
		inputTensor.emplace_back(Ort::Value::CreateTensor<float>(_memoryInfo, (float*)_blob.data, inputTensorSize, _inputNodeDims.data(),
																 _inputNodeDims.size()));
	}
	catch (Ort::Exception oe)
	{
		std::cout << "ONNX exception caught: " << oe.what() << ". Code: " << oe.GetOrtErrorCode() << ".\n";
		return false;
	}
	return true;
}

int YOLOv10::Inference(cv::Mat& image, std::vector<Ort::Value>& outputTensor)
{
	std::vector<Ort::Value> inputTensor;
	bool error = PreProcess(image, inputTensor);
	if (!error) return NULL;
	try
	{
		outputTensor = _session.Run(Ort::RunOptions{nullptr}, _inputNodeNames->data(), inputTensor.data(), inputTensor.size(),
									_outputNodeNames->data(), 1);
	}
	catch (Ort::Exception oe)
	{
		std::cout << "ONNX exception caught: " << oe.what() << ". Code: " << oe.GetOrtErrorCode() << ".\n";
		return -1;
	}
	return outputTensor.front().GetTensorTypeAndShapeInfo().GetElementCount(); // Number of elements in output = num_detected * 6
}

bool YOLOv8_OBB::PreProcess(cv::Mat& image, std::vector<Ort::Value>& inputTensor)
{
	// converting from BGR to RGB
    cv::Mat tempImg = image.clone();
    cv::cvtColor(tempImg, tempImg, cv::COLOR_BGR2RGB);

	// this will make the input into 1,3,1024,1024
	_blob = cv::dnn::blobFromImage(tempImg, 1 / 255.0, cv::Size(1024, 1024), cv::Scalar(0, 0, 0), false, false);
	size_t inputTensorSize = _blob.total();
	try
	{
		inputTensor.emplace_back(Ort::Value::CreateTensor<float>(_memoryInfo, (float*)_blob.data, inputTensorSize, _inputNodeDims.data(),
																 _inputNodeDims.size()));
	}
	catch (Ort::Exception oe)
	{
		std::cout << "ONNX exception caught: " << oe.what() << ". Code: " << oe.GetOrtErrorCode() << ".\n";
		return false;
	}
	return true;
}

int YOLOv8_OBB::Inference(cv::Mat& image, std::vector<Ort::Value>& outputTensor)
{
	std::vector<Ort::Value> inputTensor;
	bool error = PreProcess(image, inputTensor);
	if (!error) return NULL;
	try
	{
		outputTensor = _session.Run(Ort::RunOptions{nullptr}, _inputNodeNames->data(), inputTensor.data(), inputTensor.size(),
									_outputNodeNames->data(), 1);
	}
	catch (Ort::Exception oe)
	{
		std::cout << "ONNX exception caught: " << oe.what() << ". Code: " << oe.GetOrtErrorCode() << ".\n";
		return -1;
	}
	auto info = outputTensor.front().GetTensorTypeAndShapeInfo();
	return info.GetElementCount(); // Number of elements in output = num_detected * 13
}

bool YOLOv8::PreProcess(cv::Mat& image, std::vector<Ort::Value>& inputTensor)
{
	// this will make the input into 1,3,640,640
	_blob = cv::dnn::blobFromImage(image, 1 / 255.0, cv::Size(640, 640), cv::Scalar(0, 0, 0), true, false);
	size_t inputTensorSize = _blob.total();
	try
	{
		inputTensor.emplace_back(Ort::Value::CreateTensor<float>(_memoryInfo, (float*)_blob.data, inputTensorSize, _inputNodeDims.data(),
																 _inputNodeDims.size()));
	}
	catch (Ort::Exception oe)
	{
		std::cout << "ONNX exception caught: " << oe.what() << ". Code: " << oe.GetOrtErrorCode() << ".\n";
		return false;
	}
	return true;
}

int YOLOv8::Inference(cv::Mat& image, std::vector<Ort::Value>& outputTensor)
{
	std::vector<Ort::Value> inputTensor;
	bool error = PreProcess(image, inputTensor);
	if (!error) return NULL;
	try
	{
		outputTensor = _session.Run(Ort::RunOptions{nullptr}, _inputNodeNames->data(), inputTensor.data(), inputTensor.size(),
									_outputNodeNames->data(), 1);
	}
	catch (Ort::Exception oe)
	{
		std::cout << "ONNX exception caught: " << oe.what() << ". Code: " << oe.GetOrtErrorCode() << ".\n";
		return -1;
	}
	auto info = outputTensor.front().GetTensorTypeAndShapeInfo();
	return info.GetElementCount(); // Number of elements in output = num_detected * 13
}