#include <iostream>
#include "vid_utils.hpp"
#include "vanet.hpp"
#include <thread>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string.hpp> 
#include "caffe_pred.hpp"
#include <dirent.h>
#include <chrono>
#include <ctime>
#include <fstream>

using namespace std;

int port = DEFAULT_PORT;
string gateway = DEFAULT_GATEWAY;
vector<string> host_list;
//using namespace google::protobuf::io;

void info_dispatch();
void info_handler(string);

int main(int argc, char **argv) {
	/*typedef chrono::high_resolution_clock Time;
	typedef chrono::milliseconds ms;
	typedef chrono::duration<double> dur;
	typedef chrono::system_clock::time_point tp;

	caffe::Caffe::Brew mode;
	string file_name = string(argv[1]);
	if (argc >= 3 && string(argv[2]) == "cpu")
		mode = caffe::Caffe::CPU;
	else
		mode = caffe::Caffe::GPU;

    //caffe::CaffePred *caffe_pred = new caffe::CaffePred(MODEL_PATH, WEIGHT_PATH);
	caffe::Classifier *classifier = new caffe::Classifier(MODEL_PATH, WEIGHT_PATH, MEAN_PATH);
	classifier->Classify(cv::imread("videos/extracted/frame_1.jpg"), mode, 1);
	
	tp t1 = Time::now();
	clock_t c1 = clock();
	//string directory = extract_frames("VID_20150706_112355.mp4");
	vector<cv::Mat*> *imgs = new vector<cv::Mat*>();
	extract_frames(file_name, *imgs); 
	tp t2 = Time::now();
	clock_t c2 = clock();
	//process_group(directory, caffe_pred, mode, 1, 100);
	predict_frames(*imgs, classifier, mode);
	tp t3 = Time::now();
	clock_t c3 = clock();

	dur d1 = t2 - t1;
	dur d2 = t3 - t2;

	cout << "File: " << file_name << endl << endl;
	cout << "Extract time elapsed:\t" << chrono::duration_cast<ms>(d1).count() << endl;
	cout << "Extract time total:\t" << (double(c2 - c1) / CLOCKS_PER_SEC) * 1000 << endl << endl;
	cout << "Predict time elapsed:\t" << chrono::duration_cast<ms>(d2).count() << endl;
	cout << "Predict time total:\t" << (double(c3 - c2) / CLOCKS_PER_SEC) * 1000 << endl << endl << endl;

	delete imgs;
	

	delete classifier;*/


	int socket, a, b;
	string in, cmd;
	vector<string> command;

	

	cout << INPUT_PROMPT;
	getline(cin, in);

	while (true) {
		boost::trim_if(in, boost::is_any_of("\t "));
		boost::algorithm::split(command, in, boost::is_any_of("\t "), boost::token_compress_on);
		cmd = command[0];

		if (cmd == "help" || cmd == "h") {
			cout << "Available commands:\n"
			<< "->'init' or 'i'\n"
			<< "   Initialize devices on network\n"
			<< "->'query-all'\n"
			<< "   Pull all videos\n"
			<< "   (unprocessed videos may be processed on server)\n"
			<< "->'query-tags' [tags]\n"
			<< "   Pull videos associated with specified tags (space delimited)\n"
			<< "   (unprocessed videos may be processed on server)\n"
			<< "->'quit' or 'q'\n"
			<< "   Quit VAServer\n";
		}
		else if (cmd == "init" || cmd == "i")
			task_dispatch(INIT);
		else if (cmd == "quit" || cmd == "q")
			return 0;
		else if (cmd == "query-all")
			task_dispatch(QUERY_ALL);
		else
			cout << "Command not recognized\n" << "Enter 'help' for available commands\n";
		
		cout << INPUT_PROMPT;
		getline(cin, in);
	}

	//host_list.push_back(string("10.100.1.102"));

	return 0;
}

void task_dispatch(task_type task) {
	int i;
	thread *threads;
	discover_hosts();
	function funk;

	threads = new thread[host_list.size()];

	if (task == INIT) {
		for(i = 0; i < host_list.size(); i++) {
			threads[i] = thread(init_handler, host_list[i]);
		}
	}

	if (task == QUERY_ALL) {
		for(i = 0; i < host_list.size(); i++) {
			threads[i] = thread(info_handler, host_list[i]);
		}
	}

	for(i = 0; i < host_list.size(); i++) {
		threads[i].join();
	}

	delete [] threads;
}

void init_handler(string ip) {
	int client;
	vanet_pb::ServerRequest request;
	vanet_pb::ClientResponse response;
	vanet_pb::Info info;
	if ((client = socket_open(ip.c_str())) < 0) {
		return;
	}

	request.set_type(vanet_pb::ServerRequest::QUERY_ALL);
	if(!send_request(client, request)) {
		cout << "Error sending request\n";
		return;
	}

	if(!receive_response(client, response)) {
		cout << "Error reading response\n";
		return;
	}
	if(response.info_size() == 0) {
		cout << "No response from device\n";
		return;
	}

}

void info_handler(string ip) {
	int client;
	vanet_pb::ServerRequest request;
	vanet_pb::ClientResponse response;
	vanet_pb::Info info;
	if ((client = socket_open(ip.c_str())) < 0) {
		return;
	}

	request.set_type(vanet_pb::ServerRequest::QUERY_ALL);
	if(!send_request(client, request)) {
		cout << "Error sending request\n";
		return;
	}

	if(!receive_response(client, response)) {
		cout << "Error reading response\n";
		return;
	}
	if(response.info_size() == 0) {
		cout << "No response from device\n";
		return;
	}

}
