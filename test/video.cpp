#include "video.h"

using namespace std;

namespace vanet {

	Video::Video(const vanet_pb::VideoInfo &info) {
		name = info.name();
		path = info.path();
		timestamp = info.timestamp();
		size = info.size();
		duration = info.duration();
		bitrate = info.bitrate();
		mime = info.mime();
		if (info.has_loc_lat() && info.has_loc_long()) {
			loc_lat = info.loc_lat();
			loc_long = info.loc_long();
		}
		width = info.width();
		height = info.height();
		rotation = info.rotation();
		frames_processed = info.frames_processed();
		frames_total = info.frames_total();
		/*if (info.tags_size() > 0) {
			for (auto tag : info.tags())
				tags->push_back(tag);
		}*/
		mode = info.process_mode();
	}

	void Video::add_batch(int batch_size, vector<int> classifications) {
		//unique_lock<mutex> lck(vid_lck);
		//batches.push_back(make_pair(batch_size, classifications));
		//lck.unlock();
	}

	const vector<pair<int, vector<int>>>& Video::get_batches() {
		return batches;
	}

	/*void Video::add_batch(uint32_t batch_size, vector<float> classifications) {
		batches->push_back(pair<uint32_t, vector<float>>(batch_size, classifications));
		frames_processed += batch_size;
	}

	vector<float> Video::aggregate_batches() {
		if (batches->size() == 0)
			return vector<float>();
		vector<float> vec(batches[0].size());
		float total = 0;
		for (pair<uint32_t, vector<float>> batch : *batches) {
			for (int i = 0; i < batch.second.size(); i++) {
				if (vec.size() < i + 1)
					vec.push_back(batch.second[i]);
				else
					vec[i] += batch.second[i];
				total += batch.second[i];
			}
		}

		for (int i = 0; i < vec.size(); i++)
			vec[i] /= total;
	}*/
}

