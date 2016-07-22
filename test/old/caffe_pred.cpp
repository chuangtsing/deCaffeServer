#include <glog/logging.h>
#include <ctime>
#include <string>
#include <numeric>
#include "caffe_pred.hpp"

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

const char* const CaffePred::class_names[] = {"Person", "Soldier", "Army Tank",
	"Military Vehicle", "Truck", "Car", "Launcher", "Rifle", "Explosion", "Ship",
	"Helicopter", "Animal", "Background"};

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

CaffePred::CaffePred(string model_path, string weights_path) {
	CHECK_GT(model_path.size(), 0) << "Need a model definition to score.";
	CHECK_GT(weights_path.size(), 0) << "Need model weights to score.";

	Caffe::set_mode(Caffe::CPU);

	clock_t t_start = clock();
	caffe_net = new Net<float>(model_path, caffe::TEST);
	caffe_net->CopyTrainedLayersFrom(weights_path);
	clock_t t_end = clock();
}

CaffePred::~CaffePred() {
	free(caffe_net);
	caffe_net = NULL;
}

int CaffePred::test(string img_path) {
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
}

vector<int> CaffePred::predict_top_k(string img_path, Caffe::Brew mode, int k) {
	CHECK(caffe_net != NULL);

	std::cout << "step 1" << std::endl;

	Datum datum;
	Caffe::set_mode(mode);
	CHECK(ReadImageToDatum(img_path, 0, 256, 256, true, &datum));
	const shared_ptr<MemoryDataLayer<float>> memory_data_layer =
		static_pointer_cast<MemoryDataLayer<float>>(
			caffe_net->layer_by_name("data"));
	memory_data_layer->AddDatumVector(vector<Datum>({datum}));

	float loss;
	vector<Blob<float>* > dummy_bottom_vec;
	clock_t t_start = clock();
	const vector<Blob<float>*>& result = caffe_net->Forward(dummy_bottom_vec, &loss);
	clock_t t_end = clock();
	std::cout << "Time: " << (int) ((t_end - t_start) * 1000) / CLOCKS_PER_SEC << "ms\n";
	
	const vector<float> probs = vector<float>(result[1]->cpu_data(), result[1]->cpu_data() + result[1]->count());
	CHECK_LE(k, probs.size());
	vector<size_t> sorted_index = ordered(probs);

	return vector<int>(sorted_index.begin(), sorted_index.begin() + k);
}

vector<int> CaffePred::predict_top_k_mat(const cv::Mat &img, Caffe::Brew mode, int k) {
	CHECK(caffe_net != NULL);

	Datum datum;
	Caffe::set_mode(mode);
	CVMatToDatum(img, &datum);

	const shared_ptr<MemoryDataLayer<float>> memory_data_layer =
		static_pointer_cast<MemoryDataLayer<float>>(
			caffe_net->layer_by_name("data"));
	memory_data_layer->AddDatumVector(vector<Datum>({datum}));

	float loss;
	vector<Blob<float>* > dummy_bottom_vec;
	clock_t t_start = clock();
	const vector<Blob<float>*>& result = caffe_net->Forward(dummy_bottom_vec, &loss);
	clock_t t_end = clock();
	std::cout << "Time: " << (int) ((t_end - t_start) * 1000) / CLOCKS_PER_SEC << "ms\n";
	
	const vector<float> probs = vector<float>(result[1]->cpu_data(), result[1]->cpu_data() + result[1]->count());
	CHECK_LE(k, probs.size());
	vector<size_t> sorted_index = ordered(probs);

	return vector<int>(sorted_index.begin(), sorted_index.begin() + k);
}

vector<int> CaffePred::predict_top_k_batch(const vector<cv::Mat*> &imgs, Caffe::Brew mode, int k) {
	CHECK(caffe_net != NULL);

	Datum datum;
	vector<Datum> *data = new vector<Datum>();
	Caffe::set_mode(mode);
	for(int i = 0; i < imgs.size(); i++) {
		CVMatToDatum(*imgs[i], &datum);
		data->push_back(datum);
	}
	const shared_ptr<MemoryDataLayer<float>> memory_data_layer =
		static_pointer_cast<MemoryDataLayer<float>>(
			caffe_net->layer_by_name("data"));
	memory_data_layer->AddDatumVector(*data);

	float loss;
	vector<Blob<float>* > dummy_bottom_vec;
	clock_t t_start = clock();
	const vector<Blob<float>*>& result = caffe_net->Forward(dummy_bottom_vec, &loss);
	clock_t t_end = clock();
	std::cout << "Time: " << (int) ((t_end - t_start) * 1000) / CLOCKS_PER_SEC << "ms\n";
	
	const vector<float> probs = vector<float>(result[1]->cpu_data(), result[1]->cpu_data() + result[1]->count());
	CHECK_LE(k, probs.size());
	vector<size_t> sorted_index = ordered(probs);

	delete data;

	return vector<int>(sorted_index.begin(), sorted_index.begin() + k);
}
} // namespace caffe
