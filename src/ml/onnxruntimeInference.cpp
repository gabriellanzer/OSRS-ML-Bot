#include <ml/onnxruntimeInference.h>

#include <opencv2/imgproc.hpp>

#include <iostream>

bool OnnxInferenceBase::LoadModel(bool useCuda, const wchar_t* modelPath)
{
	// Initialize session options
	setSessionOptions(useCuda);

	try
	{
		printf("Loading ONNX Model from: %ls\n", modelPath);

		// Model path is const wchar_t*
		_session = Ort::Session(_env, modelPath, _sessionOptions);

		// Local allocator
		Ort::AllocatorWithDefaultOptions allocator;

		// Get input count
		assert(_session.GetInputCount() > 0);

		// Get input name
		Ort::AllocatedStringPtr inputName = _session.GetInputNameAllocated(0, allocator);

		// Get input type and shape
		Ort::TypeInfo typeInfo = _session.GetInputTypeInfo(0);
		Ort::ConstTensorTypeAndShapeInfo tensorInfo = typeInfo.GetTensorTypeAndShapeInfo();
		std::vector<int64_t> inputDims = tensorInfo.GetShape();

		printf("Input Name: %s\n", inputName.get());
		printf("Input Shape(%zu):\n", inputDims.size());
		printf("(");
		for (int i = 0; i < inputDims.size(); ++i) {
			printf("%lli", inputDims[i]);
			if (i != inputDims.size() - 1) printf(", ");
		}
		printf(")\n");

		_inputNodeDims = inputDims;
		printf("Done!\n\n");
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

void OnnxInferenceBase::setSessionOptions(bool useCuda)
{
	_sessionOptions.SetInterOpNumThreads(1);
	_sessionOptions.SetIntraOpNumThreads(1);
	// Optimization will take time and memory during startup
	_sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_DISABLE_ALL);

	// CUDA options. If used.
	if (useCuda)
	{
		_cudaOptions.device_id = 0;												// GPU_ID
		_cudaOptions.cudnn_conv_algo_search = OrtCudnnConvAlgoSearchExhaustive; // Algo to search for Cudnn
		_cudaOptions.arena_extend_strategy = 0;
		// May cause data race in some condition
		_cudaOptions.do_copy_in_default_stream = 0;
		_sessionOptions.AppendExecutionProvider_CUDA(_cudaOptions); // Add CUDA options to session options
	}
}

int YOLOInterfaceBase::Inference(cv::Mat& image, std::vector<Ort::Value>& outputTensor)
{
	std::vector<Ort::Value> inputTensor;
	bool error = preProcess(image, inputTensor);
	if (!error) return NULL;
	try
	{
		outputTensor = _session.Run(Ort::RunOptions{nullptr}, _inputNodeNames.data(),
			inputTensor.data(), inputTensor.size(), _outputNodeNames.data(), 1);
	}
	catch (Ort::Exception oe)
	{
		std::cout << "ONNX exception caught: " << oe.what() << ". Code: " << oe.GetOrtErrorCode() << ".\n";
		return -1;
	}
	auto info = outputTensor[0].GetTensorTypeAndShapeInfo();
	return info.GetElementCount();
}

void YOLOv8::Inference(cv::Mat& image, std::vector<YoloDetectionBox>& detectionBoxes)
{
	int elementCount = YOLOInterfaceBase::Inference(image, _outputTensor);
	if (elementCount == -1) return;

	_outputScaling = { (float)image.cols / _inputNodeDims[2], (float)image.rows / _inputNodeDims[3] };

	// Get the output tensor
	std::vector<int64_t> outputTensorShape = _outputTensor[0].GetTensorTypeAndShapeInfo().GetShape();
	float* outputData = _outputTensor[0].GetTensorMutableData<float>();

	// Transpose: [bs, features, preds_num] => [bs, preds_num, features]
	cv::Mat outputMat = cv::Mat(cv::Size((int)outputTensorShape[2], (int)outputTensorShape[1]), CV_32F, outputData).t();

	int geomStride = 4; // number of geometry parameters {x, y, w, h}
	int dataWidth = _classNumber + geomStride;
	int rows = outputMat.rows;
	float* pData = (float*)outputMat.data;

	// Update detections
	detectionBoxes.clear();
	for (int r = 0; r < rows; ++r)
	{
		cv::Mat scores(1, _classNumber, CV_32FC1, pData + geomStride);
		cv::Point classId;
		double maxConf;
		minMaxLoc(scores, nullptr, &maxConf, nullptr, &classId);

		if (maxConf > _confidenceThreshold)
		{
			// Extract geometry
			float xOrig = pData[0];
			float yOrig = pData[1];
			float wOrig = pData[2];
			float hOrig = pData[3];

			// Centralizing the bounding box
			float x = (xOrig - wOrig * 0.5) * _outputScaling[0];
			float y = (yOrig - hOrig * 0.5) * _outputScaling[1];
			float w = wOrig * _outputScaling[0];
			float h = hOrig * _outputScaling[1];

			detectionBoxes.push_back({ x, y, w, h, classId.x });
		}
		pData += dataWidth; // next pred
	}

	_outputTensor.clear();
}

bool YOLOv8::preProcess(cv::Mat& image, std::vector<Ort::Value>& inputTensor)
{
	int64_t _imageWidght = _inputNodeDims[2];
	int64_t _imageHeight = _inputNodeDims[3];
	// this will make the input into (1,3,_imageWidght,_imageHeight) - usually (1,3,640,640)
	_blob = cv::dnn::blobFromImage(image, 1 / 255.0, cv::Size(_imageWidght, _imageHeight), cv::Scalar(0, 0, 0), true, false);
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