#include "task_handlers.h"
#include <string>
#include <vector>
#include "vanet.h"
#include <iostream>
#include "vid_utils.h"
#include <mutex>
#include <tuple>
#include <queue>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include "algo/algo.h"
#include <signal.h>
#include <pthread.h>

using namespace std;

namespace vanet {

typedef tuple<int, string, string, long, string> vidtuple;
map<string,pair<thread, bool>> pingers;
map<int,pair<thread*, set<string>>> receivers;
vector<vanet_pb::VideoInfo> info_vec;
queue<vidtuple> videos;
queue<pair<string, vanet_pb::VideoInfo>> proc_queue;

mutex upl_lock, proc_lock, out_lock, vid_lock;
condition_variable upl_cv, proc_cv, out_cv;
bool uploading = false, proc_ready = false, processing = false;
caffe::Classifier *classifier;


void init_handler(int socket) {
	vanet_pb::ServerMessage s_message;
	vanet_pb::ClientMessage c_message;
	vanet_pb::VideoInfo info;

	s_message.set_type(vanet_pb::ServerMessage::INIT);
	s_message.add_size(0xAABB);
	if(!send_message(socket, s_message)) {
		cout << "Error sending s_message\n";
		return;
	}

	if(!receive_message(socket, c_message) || c_message.type() != vanet_pb::ClientMessage::INIT) {
		cout << "Error reading c_message\n";
		return;
	}

	send_resources(socket, c_message.resources());
}

void query_all_handler(int socket, const string &ip) {
	vanet_pb::ServerMessage s_message;
	vanet_pb::ClientMessage c_message;
	vanet_pb::VideoInfo info;
	
	s_message.set_type(vanet_pb::ServerMessage::QUERY_ALL);
	if(!send_message(socket, s_message)) {
		cout << "Error sending s_message\n";
		return;
	}

	if(!receive_message(socket, c_message)) {
		cout << "Error reading c_message\n";
		return;
	}

	cout << ip << " connection speed: " << c_message.link_speed() << endl;
	for (auto info : c_message.video_info()) {
		cout << ip << ": " << info.path() << endl;
	}
	
	c_messages.push_back(tuple<string,int,vanet_pb::ClientMessage>(ip,socket, c_message));
}


void pinger(const string &ip) {
	int socket;
	bool &connected = pingers[ip].second;
	if ((socket = open_socket(ip, DEFAULT_PING_PORT, PING_TIMEOUT, PING_TIMEOUT)) < 0) {
		connected = false;
		close(socket);
		return;
	}
	
	while (connected) {
		vanet_pb::ServerMessage smsg;
		smsg.set_type(vanet_pb::ServerMessage::PING);
		if(!send_message(socket, smsg)) {
			connected = false;
			close(socket);
			return;
		}

		vanet_pb::ClientMessage msg;
		if (!receive_message(socket, msg) || msg.type() != vanet_pb::ClientMessage::PING) {
			connected = false;
			close(socket);
			return;
		}

		if (msg.terminate() || !msg.has_terminate()) {
			close(socket);
			return;
		}
		this_thread::sleep_for(chrono::seconds(10));
	}
}

void processor() {
	//cout << "PROCESSOR STARTED: 0x" << hex << pthread_self() << endl;
	processing = true;
	unique_lock<mutex> lck(proc_lock);
	unique_lock<mutex> vid_lck(vid_lock);
	while(!(videos.empty() && proc_queue.empty())) {
		vid_lck.unlock();
		while(proc_queue.empty()) {
			proc_cv.wait(lck);
		}
		//cout << "processing\n";
		pair<string,vanet_pb::VideoInfo> vid = proc_queue.front();
		lck.unlock();
		vector<cv::Mat*> imgs;
		extract_frames(vid.first, imgs);
		vector<caffe::Prediction> prediction = predict_frames(imgs, classifier, caffe::Caffe::GPU, 13); // Change to GPU
		vanet_pb::VideoInfo info = vid.second;
		for (auto p : prediction) {
			info.add_tags(p.first);
		}
		info_vec.push_back(info);
		lck.lock();
		proc_queue.pop();
		//cout << "finished processing\n";
		vid_lck.lock();
	}
	lck.unlock();
	vid_lck.unlock();
	processing = false;
	//cout << "PROCESSOR FINISHED: 0x" << hex << pthread_self() << endl;
}

void receiver(int socket, const string &ip) {
	//bool &connected = pingers[ip].second;
	//cout << "RECEIVER STARTED: 0x" << hex << pthread_self() << " " << ip << endl;
	bool connected = true;
	
	set<string> vids = receivers[socket].second;
	while (connected && !vids.empty()) {
		vanet_pb::ClientMessage c_msg;
		if(!receive_message(socket, c_msg))
			continue;
		unique_lock<mutex> out_lck(out_lock);
		if(c_msg.type() == vanet_pb::ClientMessage::VIDEO_INFO) {
			cout << "VideoInfo received: " << c_msg.video_info(0).name() << endl;
			info_vec.push_back(c_msg.video_info(0));
			vids.erase(c_msg.video_info(0).path());
		}
		else if (c_msg.type() == vanet_pb::ClientMessage::VIDEO) {
			cout << "Receiving video: " << c_msg.video_info(0).name() << "\tSize: " << c_msg.video_info(0).size() << endl;

			unique_lock<mutex> upl_lck(upl_lock);
			string path = "videos/uploaded/";
			long received = receive_video(socket, path, c_msg);
			if (received <= 0)
				continue;
			//cout << "Video " << get<2>(videos.front()) << " received\n";
			unique_lock<mutex> proc_lck(proc_lock);
			proc_queue.push(pair<string,vanet_pb::VideoInfo>(path,c_msg.video_info(0)));
			vids.erase(c_msg.video_info(0).path());
			proc_lck.unlock();
			proc_cv.notify_all();
			uploading = false;
			upl_lck.unlock();
			upl_cv.notify_all();
		}
		out_lck.unlock();
	}
	//cout << "RECEIVER DONE: 0x" << hex << pthread_self() << " " << ip << endl;
}

void query_basic() {
	thread sender;
	int size = c_messages.size();
	//threads = new pair<thread,mutex>[size];
	for (int i = 0; i < size; i++) {
		tuple<string,int,vanet_pb::ClientMessage> msg = c_messages[i];
		vanet_pb::ServerMessage smsg;
		smsg.set_type(vanet_pb::ServerMessage::PROCESS_DIRECTIVE);
		set<string> vidset;
		int j = 0;
		for(auto info : get<2>(msg).video_info()) {
			smsg.add_path(info.path());
			if (j % 3) {
				smsg.add_process_mode(vanet_pb::SERVER);
				videos.push(vidtuple(get<1>(msg), info.path(), info.name(), info.size(), get<0>(msg)));
			}
			else
				smsg.add_process_mode(vanet_pb::MOBILE);
			vidset.insert(info.path());
			j++;
		}
		int socket = get<1>(msg);
		string ip = get<0>(msg);
		send_message(socket, smsg);
		//pingers[ip] = pair<thread,bool>(thread(pinger,get<0>(msg)), true);
		receivers[socket] = pair<thread*, set<string>>(new thread(receiver, socket, ip), vidset);
		//get<2>(threads[msg.first]) = false;
		//get<0>(threads[msg.first]) = thread(receiver, msg.first);
	}

	if(!videos.empty()) {
		classifier = new caffe::Classifier(MODEL_PATH, WEIGHT_PATH, MEAN_PATH);
		system("clear");
	}

	thread proc = thread(processor);
	unique_lock<mutex> vid_lck(vid_lock);
	while(!videos.empty()) {
		vidtuple video = videos.front();
		vid_lck.unlock();
		int sock = get<0>(video);
		vanet_pb::ServerMessage smsg;
		smsg.set_type(vanet_pb::ServerMessage::VIDEO_REQUEST);
		smsg.add_path(get<1>(video));
		send_message(sock, smsg);
		cout << "Sent request for video: " << smsg.path(0) << endl;
		unique_lock<mutex> lck(upl_lock);
		uploading = true;
		while(uploading)
			upl_cv.wait(lck);
		vid_lck.lock();
		videos.pop();
	}
	vid_lck.unlock();

	for(map<string, pair<thread,bool>>::iterator it = pingers.begin(); it != pingers.end(); ++it)
		it->second.first.join();

	for (map<int, pair<thread*, set<string>>>::iterator it = receivers.begin();
			it != receivers.end(); ++it) {
		if (it->second.first) {
			it->second.first->join();
			delete it->second.first;
		}
	}
	
	if (processing)
		cout << "Waiting for processor to finish...";

	proc.join();

	cout << "\r\033[K\n\nRESULTS:\n";
	for (auto info : info_vec) {
		cout << info.name() << " (" << info.size() / 1024 << " KB)\n"
			<< "Tags: ";
		for (auto tag : info.tags())
			cout << tag << " ";
		cout << endl << endl;
	}
}



void query_task(Scheduling sched) {
	thread sender;
	int size = c_messages.size();
	pingers.clear();
	receivers.clear();
	while (!proc_queue.empty())
		proc_queue.pop();
	while (!videos.empty())
		videos.pop();
	info_vec.clear();
/////////////////////////////////////////////////////////////////////////////////////
	vector<node *> nodes;
	
	double mobile_speed = 1024 * 1024 / 0.79;
	double server_speed = 1024 * 1024 / 0.0035;
	double transfer_speed = 1024 * 1204 * 7;
	switch (sched) {
		case OPT:
			for(int i = 0; i < size; i++) {
				map<int,double> map_rates;
				//map_rates[0] = test_speed(c_messages[i].first);
				map_rates[size] = transfer_speed;
			    node *n = new node(get<1>(c_messages[i]), get<0>(c_messages[i]), mobile_device, mobile_speed, map_rates);
				vanet_pb::ClientMessage cr = get<2>(c_messages[i]);
				for (int j = 0; j < cr.video_info_size(); j++) {
					vanet_pb::VideoInfo info = cr.video_info(j);
					n->videos.push_back(videos_tuple(info.size(), 0., cr.video_info(j).path()));
				}
				nodes.push_back(n);
			}
			nodes.push_back(new node(nodes.size(), "10.100.1.100", video_cloud, server_speed));
			for (int i = 0; i < nodes.size(); i++) {
				nodes[i]->sort();
			}
		
			for (auto v : nodes[0]->videos)
				cout << get<0>(v) << endl;			

			greedy_alg(nodes, size, 1);
			break;
		
		case MOBILE:
			for(int i = 0; i < size; i++) {
				map<int,double> map_rates;
				//map_rates[0] = test_speed(c_messages[i].first);
			    node *n = new node(get<1>(c_messages[i]), get<0>(c_messages[i]), mobile_device, mobile_speed, map_rates);
				vanet_pb::ClientMessage cr = get<2>(c_messages[i]);
				for (int j = 0; j < cr.video_info_size(); j++) {
					vanet_pb::VideoInfo info = cr.video_info(j);
					n->videos.push_back(videos_tuple(info.size(), 0., cr.video_info(j).path()));
				}
				nodes.push_back(n);
			}
			nodes.push_back(new node(nodes.size(), "10.100.1.100", video_cloud, server_speed));
			break;

		case SERVER:
			for(int i = 0; i < size; i++) {
				map<int,double> map_rates;
				//map_rates[0] = test_speed(c_messages[i].first);
			    node *n = new node(get<1>(c_messages[i]), get<0>(c_messages[i]), mobile_device, mobile_speed, map_rates);
				nodes.push_back(n);
			}
			
			nodes.push_back(new node(nodes.size(), "10.100.1.100", video_cloud, server_speed));
			
			for(int i = 0; i < size; i++) {
				vanet_pb::ClientMessage cr = get<2>(c_messages[i]);
				for (int j = 0; j < cr.video_info_size(); j++) {
					vanet_pb::VideoInfo info = cr.video_info(j);
					nodes[i]->videosto.push_back(videosto_tuple(info.size(), size, cr.video_info(j).path()));
					nodes[size]->videosfrom.push_back(videos_tuple(info.size(), i, cr.video_info(j).path()));
				}
			}
			break;
	}

	/*for (auto n : nodes) {
		cout << "NODE:\n(Videos)\n";
		for (auto v : n->videos)
			cout << get<2>(v) << endl;
		cout << "(Videosto)\n";
		for (auto v : n->videosto)
			cout << get<2>(v) << endl;
	}*/
/////////////////////////////////////////////////////////////////////////////////////

	for (int i = 0; i < size; i++) {
		node *n = nodes[i];
		int socket = n->getSocket();
		string addr = n->getIp();

		vanet_pb::ServerMessage smsg;
		smsg.set_type(vanet_pb::ServerMessage::PROCESS_DIRECTIVE);
		set<string> vidset;
		for(auto vid : n->videos) {
			string path = get<2>(vid);
			smsg.add_path(path);
			smsg.add_process_mode(vanet_pb::MOBILE);
			vidset.insert(path);
		}
		for(auto vid : n->videosto) {
			string path = get<2>(vid);
			smsg.add_path(path);
			smsg.add_process_mode(vanet_pb::SERVER);
			//videos.push(vidtuple(socket, path, path, get<0>(vid), addr));
			vidset.insert(path);
		}
		send_message(socket, smsg);
		//pingers[ip] = pair<thread,bool>(thread(pinger,get<0>(msg)), true);
		receivers[socket] = pair<thread*, set<string>>(new thread(receiver, socket, addr), vidset);
		//pthread_setname_np(get<0>(receivers[socket]).native_handle(), "receiver");
	}

	for (auto vid : nodes[size]->videosfrom) {
		int i = get<1>(vid);
		int socket = nodes[i]->getSocket();
		string path = get<2>(vid);
		long size = get<0>(vid);
		string addr = nodes[i]->getIp();
		videos.push(vidtuple(socket, path, path, size, addr));
	}

	for (auto n : nodes)
		delete n;

	if(!videos.empty() && classifier == NULL) {
		classifier = new caffe::Classifier(MODEL_PATH, WEIGHT_PATH, MEAN_PATH);
		system("clear");
	}

	thread proc = thread(processor);
	unique_lock<mutex> vid_lck(vid_lock);
	while(!videos.empty()) {
		vidtuple video = videos.front();
		vid_lck.unlock();
		int sock = get<0>(video);
		vanet_pb::ServerMessage smsg;
		smsg.set_type(vanet_pb::ServerMessage::VIDEO_REQUEST);
		smsg.add_path(get<1>(video));
		send_message(sock, smsg);
		//cout << "Sent request for \"" << smsg.path(0) << "\" to " << get<4>(video) << endl;
		unique_lock<mutex> lck(upl_lock);
		uploading = true;
		while(uploading)
			upl_cv.wait(lck);
		//cout << "Recovered from wait\n";
		videos.pop();
		vid_lck.lock();
	}
	vid_lck.unlock();
	//cout << "REQUESTS FINISHED\n";

	for(map<string, pair<thread,bool>>::iterator it = pingers.begin(); it != pingers.end(); ++it)
		it->second.first.join();

	//cout << "PINGERS JOINED\n";


	for (map<int, pair<thread*, set<string>>>::iterator it = receivers.begin();
			it != receivers.end(); ++it) {
		if (it->second.first) {
			it->second.first->join();
			delete it->second.first;
		}
	}
	
	//cout << "Join proc\n";
	//cout << "PROC JOINED\n";
	if (processing) {
		cout << "Waiting for processor to finish...";
		cout.flush();
	}

	proc.join();

	cout << "\r\033[K\n\nRESULTS:\n";
	for (auto info : info_vec) {
		cout << info.name() << " (" << info.size() / 1024 << " KB)\n"
			<< "Classifications:\n";
		for (int i = 0; i < 3; i++)
			cout << i << ") " << classifier->labels_[info.tags(i)] << endl;
		cout << endl;
	}
}

/*void query_task() {
	typedef vanet_pb::ClientMessage::VideoInfo VideoInfo;
	tuple<int,vector<VideoInfo>,vector<VideoInfo>> ads;
	double mobile_speed = 1 / 790;
	double mobile_speed = 1 / 3.5;
	vector<node *> nodes;
	int nod = c_messages.size();
	for(int i = 0; i < nod; i++) {
		map<int,double> map_rates;
		map_rates[0] = test_speed(c_messages[i].first);
		node *n = new node(i, mobile_device, mobile_speed, map_rates);
		vanet_pb::ClientReponse cr = c_messages[i].second;
		for (int j = 0; j < c_messages[i].second.video_info_size(); j++) {
			vanet_pb::CientResponse::VideoInfo info = cr.video_info()[j];
			node->videos.push_back(tuple<long,double,int>(size,0.,i));
		}
		nodes.push_back(node);
	}
	nodes.push_back(new node(nod, video_cloud, speed));


	for (i = 0; i < nodes.size(); i++)
		delete nodes[i];
}

void query_handler() {

}*/
}
