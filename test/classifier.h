#ifndef CLASSIFIER_H
#define CLASSIFIER_H

#include <vector>
#include <queue>
#include <utility>
#include <mutex>
#include <condition_variable>
#include <opencv2/core/core.hpp>
#include <thread>
#include "caffe_server.h"

namespace vanet {

	class MobileDevice;
	
	struct video {
		std::string path;
		std::string localPath;
	};

	struct queue_item {
		video *vid;
		MobileDevice *dev;
		bool entire_video;
		bool last;
		int batch;
		int topK;
		std::vector<cv::Mat*> mats;
	};

	class Classifier {
		public:
			Classifier();
			~Classifier();
			void put(queue_item *item);
			
			void start();
			void stop();

			void extract(video *vid, int k, MobileDevice *dev);


		private:
			std::thread run_thread;
			std::queue<queue_item*> proc_queue;
			std::mutex proc_lck, ext_lck;
			std::condition_variable proc_cv, ext_cv;
			bool stopping, extracting, wait_ext;

			caffe::CaffeServer *caffe;

			video take();
			void run();
			void run_extract(video *vid, int k, MobileDevice *dev);
	};
}





#endif
