#ifndef CAFFE_PRED_HPP_
#define CAFFE_PRED_HPP_

#include <string>
#include "caffe/caffe.hpp"

using std::string;

namespace caffe {

class CaffePred
{
public:
	static const Caffe::Brew GPU = Caffe::GPU;
	static const Caffe::Brew CPU = Caffe::CPU;

	static const char* const class_names[]; 

	CaffePred(string model_path, string weights_path);
	~CaffePred();

	int test(string img_path);

	vector<int> predict_top_k(string img_path, Caffe::Brew mode, int k = 3);
	vector<int> predict_top_k_mat(const cv::Mat &img, Caffe::Brew mode, int k);
	vector<int> predict_top_k_batch(const vector<cv::Mat*> &imgs, Caffe::Brew mode, int k);

private:
	Net<float> *caffe_net;
};

} // namespace caffe

#endif
