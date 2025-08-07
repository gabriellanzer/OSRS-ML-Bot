#include <ml/onnxruntimeInference.h>

#include <opencv2/imgproc.hpp>

#include <iostream>

#include <ml/onnxruntimeInference.h>

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

int PreProcessBoxDetectionBase::Inference(cv::Mat& image, std::vector<Ort::Value>& outputTensor)
{
	std::vector<Ort::Value> inputTensor;
	bool error = preProcess(image, inputTensor);
	if (!error) return NULL;
	try
	{
		outputTensor = _session.Run(Ort::RunOptions{nullptr}, _inputNodeNames.data(),
			inputTensor.data(), inputTensor.size(), _outputNodeNames.data(), _outputNodeNames.size());
	}
	catch (Ort::Exception oe)
	{
		std::cout << "ONNX exception caught: " << oe.what() << ". Code: " << oe.GetOrtErrorCode() << ".\n";
		return -1;
	}
	auto info = outputTensor[0].GetTensorTypeAndShapeInfo();
	return info.GetElementCount();
}

void YOLOv8::Inference(cv::Mat& image, std::vector<DetectionBox>& detectionBoxes)
{
	int elementCount = PreProcessBoxDetectionBase::Inference(image, _outputTensor);
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

void RF_DETR::Inference(cv::Mat& frame, std::vector<DetectionBox>& detectionBoxes)
{
	int elementCount = PreProcessBoxDetectionBase::Inference(frame, _outputTensor);
	if (elementCount == -1) return;

	_outputScaling = { (float)frame.cols / _inputNodeDims[2], (float)frame.rows / _inputNodeDims[3] };

	// Get predictions - output[0] is boxes, output[1] is logits
	float* pred_boxes = _outputTensor[0].GetTensorMutableData<float>();
	float* pred_logits = _outputTensor[1].GetTensorMutableData<float>();
	
	auto boxes_shape = _outputTensor[0].GetTensorTypeAndShapeInfo().GetShape();
	auto logits_shape = _outputTensor[1].GetTensorTypeAndShapeInfo().GetShape();
	
	int num_queries = static_cast<int>(boxes_shape[1]);  // Number of object queries
	int num_classes = static_cast<int>(logits_shape[2]); // Number of classes
	
	// Apply sigmoid to logits to get probabilities
	std::vector<float> probs(num_queries * num_classes);
	for (int i = 0; i < num_queries * num_classes; ++i) {
		probs[i] = 1.0f / (1.0f + std::exp(-pred_logits[i]));
	}
	
	// Select top 300 detections
	const int num_select = std::min(300, num_queries * num_classes);
	std::vector<std::pair<float, int>> prob_index_pairs;
	
	for (int i = 0; i < num_queries * num_classes; ++i) {
		prob_index_pairs.emplace_back(probs[i], i);
	}
	
	// Sort by probability (descending)
	std::partial_sort(prob_index_pairs.begin(), prob_index_pairs.begin() + num_select,
					  prob_index_pairs.end(), 
					  [](const auto& a, const auto& b) { return a.first > b.first; });
	
	// Process top detections
	detectionBoxes.clear();
	for (int i = 0; i < num_select; ++i) {
		float score = prob_index_pairs[i].first;
		int topk_index = prob_index_pairs[i].second;
		
		if (score < _confidenceThreshold) continue;
		
		int box_idx = topk_index / num_classes;
		int class_id = topk_index % num_classes;
		
		// Get box coordinates (center_x, center_y, width, height)
		float cx = pred_boxes[box_idx * 4 + 0];
		float cy = pred_boxes[box_idx * 4 + 1];
		float w = pred_boxes[box_idx * 4 + 2];
		float h = pred_boxes[box_idx * 4 + 3];
		
		// Clamp width and height to minimum 0
		w = std::max(0.0f, w);
		h = std::max(0.0f, h);
		
		// Convert from center-width-height to x1y1x2y2
		float x1 = cx - 0.5f * w;
		float y1 = cy - 0.5f * h;
		float x2 = cx + 0.5f * w;
		float y2 = cy + 0.5f * h;
		
		// Scale from relative [0,1] to absolute coordinates
		x1 *= frame.cols;
		y1 *= frame.rows;
		x2 *= frame.cols;
		y2 *= frame.rows;
		
		// Convert back to x,y,w,h format for DetectionBox
		float final_x = x1;
		float final_y = y1;
		float final_w = x2 - x1;
		float final_h = y2 - y1;
		
		DetectionBox box;
		box.x = final_x;
		box.y = final_y;
		box.w = final_w;
		box.h = final_h;
		box.classId = class_id;
		box.confidence = score;
		
		detectionBoxes.push_back(box);
	}
	
	_outputTensor.clear();
}

bool RF_DETR::preProcess(cv::Mat& frame, std::vector<Ort::Value>& inputTensor)
{
	int64_t _imageWidth = _inputNodeDims[2];
	int64_t _imageHeight = _inputNodeDims[3];
	
	// Resize the image to the required dimensions
	cv::Mat resized;
	cv::resize(frame, resized, cv::Size(_imageWidth, _imageHeight));
	
	// Convert to float and normalize to [0, 1]
	cv::Mat floatImg;
	resized.convertTo(floatImg, CV_32F, 1.0 / 255.0);
	
	// Apply ImageNet normalization (means and stds)
	cv::Scalar means(0.485, 0.456, 0.406);
	cv::Scalar stds(0.229, 0.224, 0.225);
	
	cv::subtract(floatImg, means, floatImg);
	cv::divide(floatImg, stds, floatImg);
	
	// Create blob with NCHW format
	_blob = cv::dnn::blobFromImage(floatImg, 1.0, cv::Size(_imageWidth, _imageHeight), cv::Scalar(0, 0, 0), true, false);
	
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