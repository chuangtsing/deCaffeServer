#ifndef VIDEO_H
#define VIDEO_H

#include <string>
#include <vector>
#include <utility>
#include <mutex>
#include "protobuf/vatest_proto.pb.h"

namespace vanet {

	class Video {
		private:
			std::vector<std::pair<int, std::vector<int>>> batches;
			std::mutex vid_lck;
			//std::vector<std::pair<int, std::vector<int>>>;
	
		public:
			//Video() {};
			Video(const vatest_proto::VideoInfo &info);
			Video(const Video& vid);
			//~Video() { delete batches; };
	
			//bool has_loc() { return loc; };
			//void add_batch(uint32_t batch_size, std::vector<float> classifications); // Add frame number 'frame' with 'classifications'
			//std::vector<float> aggregate_batches();
			void add_batch(int batch_size, std::vector<int> classification);
			const std::vector<std::pair<int, std::vector<int>>>& get_batches();

			//bool has_tags() { return tags == NULL ? false : true; };
			//std::vector<uint32_t>* get_tags() { return tags; };
			//void set_tags(std::vector<uint32_t> *new_tags) { delete tags; tags = new_tags; };

			std::string name;
			std::string path;
			std::string local_path;
			std::string timestamp;
			uint64_t size;
			uint64_t duration;
			uint32_t bitrate;
			std::string mime;
			double loc_lat;
			double loc_long;
			uint32_t width;
			uint32_t height;
			uint32_t rotation;
			uint32_t frames_processed;
			uint32_t frames_total;
			vatest_proto::ProcessMode mode;

			double time_predicted;
			double time_actual;
			double energy_predicted;
			double energy_actual;
	};
}
#endif
