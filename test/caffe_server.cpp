#include <string>
#include "caffe_server.h"
#include "test.h"
#include <iostream>

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/highgui/highgui_c.h>
#include <opencv2/imgproc/imgproc.hpp>
#include <caffe/layers/memory_data_layer.hpp>

using std::string;
using std::static_pointer_cast;
using std::clock;
using std::clock_t;

using caffe::Blob;
using caffe::Caffe;
using caffe::Datum;
using caffe::Net;
using caffe::shared_ptr;
using caffe::vector;
using caffe::MemoryDataLayer;


namespace caffe {

template <typename T>
vector<size_t> ordered(vector<T> const& values) {
	vector<size_t> indices(values.size());
	std::iota(begin(indices), end(indices), static_cast<size_t>(0));

	std::sort(
		begin(indices), end(indices),
		[&](size_t a, size_t b) { return values[a] > values[b]; }
	);
	return indices;
}

CaffeServer::CaffeServer(string model_path, string weights_path) {
	std::cout << "Loading model\n";
	CHECK_GT(model_path.size(), 0) << "Need a model definition to score.";
	CHECK_GT(weights_path.size(), 0) << "Need model weights to score.";

	Caffe::set_mode(Caffe::GPU);

	clock_t t_start = clock();
	caffe_net = new Net<float>(model_path, caffe::TEST);
	caffe_net->CopyTrainedLayersFrom(weights_path);
	clock_t t_end = clock();
	system("clear");
}

CaffeServer::CaffeServer() {
	string model_path = MODEL_PATH;
	string weights_path = WEIGHTS_PATH;
	CHECK_GT(model_path.size(), 0) << "Need a model definition to score.";
	CHECK_GT(weights_path.size(), 0) << "Need model weights to score.";

	Caffe::set_mode(Caffe::CPU);

	clock_t t_start = clock();
	caffe_net = new Net<float>(model_path, caffe::TEST);
	caffe_net->CopyTrainedLayersFrom(weights_path);
	clock_t t_end = clock();

	std::cout << "Finished loading\n";
}

CaffeServer::~CaffeServer() {
	free(caffe_net);
	caffe_net = NULL;
}

/*int CaffeServer::test(string img_path) {
	CHECK(caffe_net != NULL);

	Datum datum;
	CHECK(ReadImageToDatum(img_path, 0, 256, 256, true, &datum));
	const shared_ptr<MemoryDataLayer<float>> memory_data_layer =
		static_pointer_cast<MemoryDataLayer<float>>(
			caffe_net->layer_by_name("data"));
	memory_data_layer->AddDatumVector(vector<Datum>({datum}));

	vector<Blob<float>* > dummy_bottom_vec;
	float loss;
	clock_t t_start = clock();
	const vector<Blob<float>*>& result = caffe_net->Forward(dummy_bottom_vec, &loss);
	clock_t t_end = clock();

	const float* argmaxs = result[1]->cpu_data();
	for (int i = 0; i < result[1]->num(); i++) {
		for (int j = 0; j < result[1]->height(); j++) {
			LOG(INFO) << " Image: "<< i << " class:"
			          << argmaxs[i*result[1]->height() + j];
		}
	}

	return argmaxs[0];
}*/
    
/*vector<int> CaffeServer::predict_top_k(cv::Mat* matdata, int k) {
    CHECK(caffe_net != NULL);
        
    Datum datum;
//    cv::Mat img(cv::Size(256,256), CV_8U, matdata);
    cv::Mat image;
//    cv::Mat image(cvRound(256), cvRound(256), CV_8U);
    //cv::resize(*matdata, image, cv::Size(256,256));
    CVMatToDatum(image, &datum);
    const shared_ptr<MemoryDataLayer<float>> memory_data_layer =
    static_pointer_cast<MemoryDataLayer<float>>(
                                                    caffe_net->layer_by_name("data"));
    memory_data_layer->AddDatumVector(vector<Datum>({datum}));
        
    float loss;
    vector<Blob<float>* > dummy_bottom_vec;
    clock_t t_start = clock();
    const vector<Blob<float>*>& result = caffe_net->Forward(dummy_bottom_vec, &loss);
    clock_t t_end = clock();
        
    const vector<float> probs = vector<float>(result[1]->cpu_data(), result[1]->cpu_data() + result[1]->count());
    CHECK_LE(k, probs.size());
    vector<size_t> sorted_index = ordered(probs);
        
    return vector<int>(sorted_index.begin(), sorted_index.begin() + k);
}*/

//vector<int> CaffeServer::predict_top_k(vector<cv::Mat*> vecMat, int k) {
//	CHECK(caffe_net != NULL);
//
//	vector<Datum> batch;
//	for(int i=0; i< vecMat.size(); i++) {
//		Datum datum;
//		cv::Mat image;
//		cv::resize(*(vecMat[i]), image, cv::Size(256, 256));
//		CVMatToDatum(image, &datum);
//		batch.push_back(datum);
//	}
//	const shared_ptr <MemoryDataLayer<float>> memory_data_layer =
//		static_pointer_cast < MemoryDataLayer < float >> (
//				caffe_net->layer_by_name("data"));
//    memory_data_layer->set_batch_size(batch.size());
//	memory_data_layer->AddDatumVector(batch);
//
//	float loss;
//	vector<Blob<float>* > dummy_bottom_vec;
//	clock_t t_start = clock();
//	const vector<Blob<float>*>& result = caffe_net->Forward(dummy_bottom_vec, &loss);
//	clock_t t_end = clock();
//	LOG(DEBUG) << "Prediction time: " << 1000.0 * (t_end - t_start) / CLOCKS_PER_SEC << " ms.";
//
//	const vector<float> probs = vector<float>(result[1]->cpu_data(), result[1]->cpu_data() + result[1]->count());
//	CHECK_LE(k, probs.size());
//	vector<size_t> sorted_index = ordered(probs);
//
//	return vector<int>(sorted_index.begin(), sorted_index.begin() + k);
//}

std::vector<int> CaffeServer::predict_top_k(std::vector<cv::Mat*> &vecMat, int k) {
	std::vector<size_t> vec = ordered(predict(vecMat));
	return std::vector<int>(vec.begin(), vec.begin() + k);
}

std::vector<float> CaffeServer::predict(std::vector<cv::Mat*> &vecMat) {
    CHECK(caffe_net != NULL);

    vector<Datum> images;
    int batch_size = vecMat.size();
    for(int i=0; i< batch_size; i++) {
			Datum datum;
			CVMatToDatum(*vecMat[i], &datum);
      images.push_back(datum);
    }
    
    const shared_ptr <MemoryDataLayer<float>> memory_data_layer =
                            static_pointer_cast < MemoryDataLayer < float >> (caffe_net->layer_by_name("data"));
    memory_data_layer->set_batch_size(batch_size);
    memory_data_layer->AddDatumVector(images);
        
    float loss;
    vector<Blob<float>* > dummy_bottom_vec;
    clock_t t_start = clock();
//    caffe_net->ForwardPrefilled(&loss);
    caffe_net->Forward(dummy_bottom_vec, &loss);
    clock_t t_end = clock();
    
    shared_ptr<Blob<float>> prob = caffe_net->blob_by_name("prob");
    
    int increment = prob->count()/batch_size;
		std::vector<float> vecProb;
    for (int i=0; i<batch_size; i++) {
        vector<float> one = vector<float>(prob->cpu_data()+i*increment, prob->cpu_data()+(i+1)*increment);
				if (vecProb.empty())
					vecProb.assign(one.size(), 0);
				for (int j = 0; j < one.size(); j++) {
					vecProb[j] += one[j];
				}
    }

		return vecProb;
}

std::vector<int> CaffeServer::compile_top_k(std::vector<std::vector<float>> &vecProb, int k) {
	int size = vecProb[0].size();
	std::vector<float> vecTotal(size);
	vecTotal.assign(size, 0);
	for (int i = 0; i < vecProb.size(); i++) {
		for (int j = 0; j < size; j++) {
			vecTotal[j] += vecProb[i][j];
		}
	}
	
	size = k < size ? k : size;

		std::vector<size_t> order = ordered(vecTotal);
		std::vector<int> vecReturn(size);
		for (int i = 0; i < size; i++) {
			vecReturn[i] = order[i];
		}

		return vecReturn;
}


	void CaffeServer::CVMatToDatum(const cv::Mat& cv_img, Datum* datum) {
		CHECK(cv_img.depth() == CV_8U) << "Image data type must be unsigned byte";
		datum->set_channels(cv_img.channels());
		datum->set_height(cv_img.rows);
		datum->set_width(cv_img.cols);
		datum->clear_data();
		datum->clear_float_data();
		datum->set_encoded(false);
		int datum_channels = datum->channels();
		int datum_height = datum->height();
		int datum_width = datum->width();
		int datum_size = datum_channels * datum_height * datum_width;
		std::string buffer(datum_size, ' ');
		for (int h = 0; h < datum_height; ++h) {
			const uchar* ptr = cv_img.ptr<uchar>(h);
			int img_index = 0;
			for (int w = 0; w < datum_width; ++w) {
				for (int c = 0; c < datum_channels; ++c) {
					int datum_index = (c * datum_height + h) * datum_width + w;
					buffer[datum_index] = static_cast<char>(ptr[img_index++]);
				}
			}
		}
		datum->set_data(buffer);
	}

} // namespace caffe
