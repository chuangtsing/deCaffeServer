#ifndef CAFFE_MOBILE_HPP_
#define CAFFE_MOBILE_HPP_

#define CPU_ONLY

#include <string>
//#include <caffe/caffe.hpp>
#include <caffe/caffe.hpp>
#include <caffe/util/io.hpp>
#include <vector>

using std::string;

namespace caffe {

class CaffeServer
{
public:
	CaffeServer(string model_path, string weights_path);
	CaffeServer();
	~CaffeServer();

	int test(string img_path);

	std::vector<int> predict_top_k(string img_path, int k);
	std::vector<int> predict_top_k(cv::Mat* img, int k);
	std::vector<int> predict_top_k(std::vector<cv::Mat*> &vecMat, int k);
	std::vector<float> predict(std::vector<cv::Mat*> &vecMat);
	std::vector<int> compile_top_k(std::vector<std::vector<float>> &vecProb, int k);
private:
	Net<float> *caffe_net;
	void CVMatToDatum(const cv::Mat& cv_img, Datum* datum);
};

} // namespace caffe

#endif
