#ifndef VID_UTILS_H
#define VID_UTILS_H

#include "classifier.hpp"
#include <string>
#include "caffe_pred.hpp"

#define VIDEOS_PATH "videos/"
#define FRAMES_PATH "videos/extracted/"
#define FRAMES_EXT "_frames"

std::string extract_frames(std::string filename);
bool extract_group(std::string name, std::string dir);
bool extract_group_mat(std::string name, std::vector<cv::Mat*> &imgs);
bool process_group(std::string directory, caffe::CaffePred *caffe_pred, caffe::Caffe::Brew mode, int begin, int end);
bool process_group_batch(const std::vector<cv::Mat*> &imgs, caffe::CaffePred *caffe_pred, caffe::Caffe::Brew mode);
bool process_group_single(const std::vector<cv::Mat*> &imgs, caffe::CaffePred *caffe_pred, caffe::Caffe::Brew mode);
bool process_classifier(const std::vector<cv::Mat*> &imgs, caffe::Classifier *classifier, caffe::Caffe::Brew mode);
inline bool has_suffix (const std::string &s, const std::string &suffix);
#endif
