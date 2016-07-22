#ifndef CLASSIFIER_H
#define CLASSIFIER_H

#include <opencv2/opencv.hpp>
#include <caffe/caffe.hpp>
#include <string>
#include <vector>

namespace caffe {  // NOLINT(build/namespaces)

	/* Pair (label, confidence) representing a prediction. */
	typedef std::pair<int, float> Prediction;

	class Classifier {
 		public:
  			Classifier(const std::string& model_file,
            				const std::string& trained_file,
             				const std::string& mean_file
             				/*const string& label_file*/);

			static std::vector<int> Argmax(const std::vector<float>& v, int N);

			std::vector<Prediction> Classify(const cv::Mat& img, Caffe::Brew mode, int N);
			void ClassifyCollective(const cv::Mat &img, Caffe::Brew mode, std::vector<float> &vec);
			std::vector<Prediction> CompileCollective(const std::vector<float> &vec, int N);
			std::vector<float> Predict(const cv::Mat& img, Caffe::Brew mode);
			
			static const std::vector<std::string> labels_;


		private:
			static bool PairCompare(const std::pair<float, int>& lhs,
							const std::pair<float, int>& rhs);

			void SetMean(const std::string& mean_file);

			void WrapInputLayer(std::vector<cv::Mat>* input_channels);

			void Preprocess(const cv::Mat& img,
							  std::vector<cv::Mat>* input_channels);

			shared_ptr<Net<float> > net_;
			cv::Size input_geometry_;
			int num_channels_;
			cv::Mat mean_;
	};

}

#endif
