#include "classifier.h"
#include "test.h"
#include <thread>
#include "protobuf/vatest_proto.pb.h"
#include "mobile_device.h"
#include <opencv2/opencv.hpp>

using namespace std;
using namespace cv;

namespace vanet {

	Classifier::Classifier() {
		string model = MODEL_PATH;
		string weights = WEIGHTS_PATH;
		caffe = new caffe::CaffeServer(model, weights);
		extracting = false;
	}

	Classifier::~Classifier() {
		delete caffe;
	}

	void Classifier::start() {
		stopping = false;
		run_thread = thread(&Classifier::run, this);
	}

	void Classifier::stop() {
		stopping = true;
		proc_cv.notify_all();
		run_thread.join();
		unique_lock<mutex> lck(ext_lck);
		while (extracting)
			ext_cv.wait(lck);
		lck.unlock();
		while (!proc_queue.empty()) {
			queue_item *item = proc_queue.front();
			delete item->vid;
			for (cv::Mat *mat : item->mats)
				delete mat;
			proc_queue.pop();
		}
	}

	void Classifier::run() {
		vector<vector<float>> probs;
		while (true) {
			unique_lock<mutex> lck(proc_lck);
			while(!stopping && proc_queue.empty())
				proc_cv.wait(lck);
			if (stopping) {
				lck.unlock();
				return;
			}

			cout << "CLASSIFIER ITEM\n";
			queue_item *item = proc_queue.front();
			proc_queue.pop();
			lck.unlock();

			if (item->vid == NULL && item->last) {
				item->dev->close_device();
				delete item;
				cout << "Classifier disconnect\n";
				continue;
			}


			if (item->entire_video) {
				cout << "Classify frame\n";
				vector<float> prob = caffe->predict(item->mats);
				probs.push_back(prob);
				vector<int> clazzes = caffe->compile_top_k(probs, item->topK);
				for (int i: clazzes)
					cout << i << endl;
				if (item->last) {
					vector<int> classes = caffe->compile_top_k(probs, item->topK);
					vatest_proto::ServerMessage msg;
					msg.set_type(vatest_proto::ServerMessage::VIDEO);
					msg.set_path(item->vid->path);
					for (int i = 0; i < classes.size(); i++) {
						cout << classes[i] << endl;
						msg.add_tags(classes[i]);
					}
					item->dev->send_message(msg);
					delete item->vid;
				}
			}
			else {
				vector<int> classes = caffe->predict_top_k(item->mats, item->topK);
				vatest_proto::ServerMessage msg;
				msg.set_type(vatest_proto::ServerMessage::BATCH);
				msg.set_batch(item->batch);
				msg.set_path(item->vid->path);
				for (int i = 0; i < classes.size(); i++) {
					cout << classes[i] << endl;
					msg.add_tags(classes[i]);
				}
				item->dev->send_message(msg);
			}
			delete item;
		}
	}


	void Classifier::put(queue_item *item) {
		if (item->vid == NULL && item->last) {
			unique_lock<mutex> lck(ext_lck);
			while (wait_ext)
				ext_cv.wait(lck);
			lck.unlock();
		}

		unique_lock<mutex> lck(proc_lck);

		if(stopping) {
			lck.unlock();
			return;
		}

		proc_queue.push(item);

		lck.unlock();
		proc_cv.notify_all();
	}

	void Classifier::extract(video* vid, int k, MobileDevice *dev) {
		wait_ext = true;
		thread t(&Classifier::run_extract, this, vid, k, dev);
		t.detach();
	}

	void Classifier::run_extract(video *vid, int k, MobileDevice *dev) {
		//double fps;
		vector<int> params;
		int end;
		cv::Mat img;
		cv::Mat *mat;

		unique_lock<mutex> lck(ext_lck);
		while (extracting)
			ext_cv.wait(lck);
		extracting = true;
		lck.unlock();

		cv::VideoCapture cap(vid->localPath);
		if (!cap.isOpened()) {
			cout << "Unable to open video: " << vid->localPath << endl;
			return;
		}

		end = cap.get(CV_CAP_PROP_FRAME_COUNT);
		
		params.push_back(CV_IMWRITE_JPEG_QUALITY);
		params.push_back(100);


		int i = 1;
		int frame = 0;

		while (!stopping && cap.read(img)) {
			mat = new cv::Mat(img.clone());
			queue_item *item = new queue_item;
			item->entire_video = true;
			item->mats.push_back(mat);
			item->vid = vid;
			item->topK = k;
			item->dev = dev;
			
			i++;

			if ((frame += 30) > end) {
				item->last = true;
				put(item);
				break;
			}
		
			item->last = false;
			put(item);
			cap.set(CV_CAP_PROP_POS_FRAMES, frame);
		}

		extracting = false;
		wait_ext = false;
		ext_cv.notify_all();
		return;
	}
}
