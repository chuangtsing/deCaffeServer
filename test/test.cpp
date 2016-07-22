#include <iostream>
#include <thread>
#include <dirent.h>
#include <chrono>
#include <ctime>
#include <fstream>
#include "mobile_device.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <list>
#include <queue>
#include "classifier.h"
#include "test.h"


#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include "caffe_server.h"



using namespace std;
using namespace vanet;

Classifier classifier;
list<MobileDevice*> devices;

int s_sock;


void int_handler(int sig) {
	cout << "\rInterrupt received, stopping server\n";
	close(s_sock);
	for (list<MobileDevice*>::iterator it = devices.begin(); it != devices.end(); it++) {
		(*it)->close_device();
		delete *it;
	}
	devices.clear();
	classifier.stop();
	exit(0);
}

void client_handler(string ip, int socket) {
	MobileDevice *dev = new MobileDevice(ip, socket);
	
	if (!dev->establish_connection()) {
		cout << "Error establishing connection to " << ip << endl;
		return;
	}

	cout << "Connected to " << ip << endl;

	devices.push_back(dev);

	dev->start_receiver();

	cout << "Successfully disconnected from " << ip << endl;
	devices.remove(dev);
	//delete dev;
}

int main(int argc, char **argv) {
	/*caffe::CaffeServer *caffe = new caffe::CaffeServer(MODEL_PATH, WEIGHTS_PATH);
	cv::Mat mat = cv::imread("pic.jpg", CV_LOAD_IMAGE_COLOR);
	vector<int> vec = caffe->predict_top_k(&mat, 10);
	cout << "Classifications\n";
	for (int i = 0; i < vec.size(); i++) {
		cout << vec[i] << endl;
	}*/
	
	int pId, port, c_sock, reuseaddr = 1;
	socklen_t len;
	struct sockaddr_in server, client;
	string ip;

	signal(SIGINT, int_handler);

	s_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (s_sock < 0) {
		"Could not open socket\n";
		return -1;
	}

	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(DEFAULT_PORT);


	setsockopt(s_sock, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(int));

	if (bind(s_sock, (struct sockaddr *) &server, sizeof(server)) < 0 ) {
		cout << "Could not bind to socket\n";
		return -1;
	}

	listen(s_sock, 5);

	cout << "Server started, ctrl-c to stop\n";

	classifier.start();

	while(true) {
		len = sizeof(client);
		c_sock = accept(s_sock, (struct sockaddr *) &client, &len);
		if (c_sock < 0) {
			cout << "Could not connect to client\n";
			continue;
		}
		thread (client_handler, string(inet_ntoa(client.sin_addr)), c_sock).detach();
	}

	classifier.stop();

	return 0;
}
