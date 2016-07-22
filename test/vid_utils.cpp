#include "classifier.h"
//#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include <sys/stat.h>
#include "vanet.h"
#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>
#include "vid_utils.h"
#include <dirent.h>
#include <regex>
#include <thread>

using namespace std;

namespace vanet {
extern Classifier classifier;

bool extract_frames(video *vid) {
	//double fps;
	vector<int> params;
	string vid;
	int end;
	cv::Mat img;
	cv::Mat *mat;

	cv::VideoCapture cap(vid->localPath);
	if (!cap.isOpened()) {
		cout << "Unable to open video: " << name << endl;
		return false;
	}

	end = cap.get(CV_CAP_PROP_FRAME_COUNT);
	
	params.push_back(CV_IMWRITE_JPEG_QUALITY);
	params.push_back(100);


	int i = 1;
	int frame = 0;
	while (cap.read(img)) {
		mat = new cv::Mat(img.clone());
		vid.mats.push_back(mat);
		queue_item *item = new queue_item;
		item->entire_video = true;
		
		i++;

		if ((frame += 30) > end) {
			item->last = true;
			classifier.put(item);
			break;
		}
	
		item->last = false;
		classifier.put(item);
		cap.set(CV_CAP_PROP_POS_FRAMES, frame);
	}

	return true;
}


/*std::vector<caffe::Prediction> predict_frames(const vector<cv::Mat*> &imgs, caffe::Classifier *classifier, caffe::Caffe::Brew mode, int N) {
	std::vector<float> total;
	for (auto img : imgs) {
		CHECK(!img->empty()) << "Unable to decode image";
		classifier->ClassifyCollective(*img, mode, total);
		delete img;
	}

	std::vector<caffe::Prediction> predictions = classifier->CompileCollective(total, N);
	// Print the top N predictions.
	for (size_t i = 0; i < predictions.size(); ++i) {
		caffe::Prediction p = predictions[i];
		std::cout << std::fixed << std::setprecision(4) << p.second << " - \""
		<< p.first << "\"" << std::endl;
	}
	
	return predictions;

}*/
}
