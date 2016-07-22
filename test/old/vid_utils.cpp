#include "classifier.hpp"
//#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include <sys/stat.h>
#include "vanet.hpp"
#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>
#include "vid_utils.hpp"
#include <dirent.h>
#include <caffe_pred.hpp>
#include <regex>
#include <thread>

using namespace std;

/*struct file_comp {
	bool operator()(const string &f1, const string &f2) {
		boost::regex exp("[0-9]+", regex_constants::basic);
		boost::smatch sm1, sm2;

		boost::regex_search(f1, sm1, exp);
		boost::regex_search(f2, sm2, exp);
		return stoi(sm1[1].first) >= stoi(sm2[1].first);
	}
};*/

string extract_frames(string filename) {
	string dir_base = FRAMES_PATH;
	string directory;
	string vid = VIDEOS_PATH + filename;

	int begin, end;
	int slash = filename.find_last_of("/");
	if (begin != string::npos)
		begin = slash + 1;
	else
		begin = 0;
	int period = filename.find_last_of(".");
	if (period != string::npos)
		end = period;
	else
		end = filename.size();
		

	dir_base += filename.substr(begin, end);

	int i = 1;
	int mode = S_IRWXU | S_IRWXG | S_IRWXO;//S_IRUSR | S_IWUSR; //| S_IRGRP | S_IWGRP;
	directory = dir_base;
	vaout("Directory: %s\n", dir_base.c_str());
	int ret = mkdir(directory.c_str(), mode);
	while (ret == -1) {
		if(errno == EEXIST) {
			directory = dir_base + "_" + boost::lexical_cast<std::string>(i);
			ret = mkdir(directory.c_str(), mode);
			i++;
		}
		else {
			vaout("Failed to create frame directory (Errno: %d)\n", errno);
			return NULL;
		}
	}

	directory += "/";

	if (!extract_group(filename, directory))
		vaout("Error extracting frames\n");


	return directory;
}

bool extract_group(string name, string dir) {
	//long num = first;
	//double fps;
	//cv::Mat img;
	vector<int> params;
	string vid;
	int end;
	cv::Mat img;

	cv::VideoCapture cap(name);
	if (!cap.isOpened()) {
		vaout("Unable to open video: %s\n", name.c_str());
		return false;
	}

	end = cap.get(CV_CAP_PROP_FRAME_COUNT);
	
	params.push_back(CV_IMWRITE_JPEG_QUALITY);
	params.push_back(100);

	int i = 1;
	int frame = 0;
	string file_base = dir + "frame_";
	while (cap.read(img)) {
		vid = file_base + boost::lexical_cast<std::string>(i) + ".jpg";
		try {
			vaout((vid + "\n").c_str());
			cv::imwrite(vid, img, params);
		}
		catch (runtime_error& ex) {
			vaout("Exception: %s\n", ex.what());
			return false;
		}
		i++;
		frame += 30;
		if (frame > end)
			break;
		cap.set(CV_CAP_PROP_POS_FRAMES, frame);
	}

	return true;

}

void extract(string name, int begin, int end, vector<cv::Mat*> &imgs) {
	
}

bool extract_group_mat(string name, vector<cv::Mat*> &imgs) {
	//long num = first;
	//double fps;
	//cv::Mat img;
	vector<int> params;
	string vid;
	int end;
	cv::Mat img;
	cv::Mat *mat;

	cv::VideoCapture cap(/*"videos/" + */name);
	if (!cap.isOpened()) {
		vaout("Unable to open video: %s\n", name.c_str());
		return false;
	}

	end = cap.get(CV_CAP_PROP_FRAME_COUNT);
	
	params.push_back(CV_IMWRITE_JPEG_QUALITY);
	params.push_back(100);


	int i = 1;
	int frame = 0;
	while (cap.read(img)) {
		cout << "Frame: " << i << endl;
		mat = new cv::Mat(img.clone());
		imgs.push_back(mat);
		i++;
		frame += 30;
		if (frame > end)
			break;
		cap.set(CV_CAP_PROP_POS_FRAMES, frame);
	}

	return true;
}

bool process_group(string directory, caffe::CaffePred *caffe_pred, caffe::Caffe::Brew mode, int begin, int end) {
	DIR *dp;
	struct dirent *dirp;
	vector<string> files;
	//regex reg(".*\\.jpg");
	string file_base = directory + "frame_";

	for(int i = begin; i <= end; i++) {
		string file = file_base + boost::lexical_cast<std::string>(i) + ".jpg";
		ifstream f(file);
		if(!f.good()) {
			f.close();
			break;
		}
		f.close();

		vector<int> features = caffe_pred->predict_top_k(file, mode, 1);
		cout << "File: " << file << endl;
		for(auto k : features)
			cout << "Predict: " << k << endl;
	}
	return true;
}

bool process_group_batch(const vector<cv::Mat*> &imgs, caffe::CaffePred *caffe_pred, caffe::Caffe::Brew mode) {
	vector<int> features = caffe_pred->predict_top_k_batch(imgs, mode, 1);
	cout << "Predictions: "  << endl;
	for(auto k : features)
		cout << "Predict: " << k << endl;
	return true;
}

bool process_group_single(const vector<cv::Mat*> &imgs, caffe::CaffePred *caffe_pred, caffe::Caffe::Brew mode) {
	for (auto img : imgs) {
		//vector<int> features = caffe_pred->predict_top_k_mat(*img, mode, 10);
		
		//cout << "Predictions: "  << endl;
		/*for(auto k : features)
			cout << "Predict: " << k << endl;*/
	}
	return true;
}

bool process_classifier(const vector<cv::Mat*> &imgs, caffe::Classifier *classifier, caffe::Caffe::Brew mode) {
	for (auto img : imgs) {
		//vector<int> features = caffe_pred->predict_top_k_mat(*img, mode, 10);
		CHECK(!img->empty()) << "Unable to decode image";
		std::vector<caffe::Prediction> predictions = classifier->Classify(*img, mode, 1);

		// Print the top N predictions.
		cout << "Image\n";
		for (size_t i = 0; i < predictions.size(); ++i) {
			caffe::Prediction p = predictions[i];
			std::cout << std::fixed << std::setprecision(4) << p.second << " - \""
			<< p.first << "\"" << std::endl;
		}
		//cout << "Predictions: "  << endl;
		/*for(auto k : features)
			cout << "Predict: " << k << endl;*/
	}
	return true;

}

bool has_suffix(const string &s, const string &suffix) {
	return ((s.size() >= suffix.size()) && equal(suffix.rbegin(), suffix.rend(), s.rbegin()));
}
