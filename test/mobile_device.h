#ifndef MOBILE_DEVICE_H
#define MOBILE_DEVICE_H

#include <string>
#include <vector>
#include <unistd.h>
#include <thread>
#include <queue>
#include <vector>
#include "protobuf/vatest_proto.pb.h"
#include <chrono>
#include "mobile_device.h"
#include <opencv2/core/core.hpp>
#include <mutex>
#include <condition_variable>
#include "classifier.h"

//extern std::queue<std::pair<vanet::Video*, vanet::MobileDevice*>> proc_queue;
//extern std::vector<vanet::Video*> finished_vec;

namespace vanet {


typedef std::chrono::high_resolution_clock Time;
typedef std::chrono::milliseconds MS;
typedef std::chrono::duration<double> Duration;
typedef std::chrono::system_clock::time_point TimePoint;

class MobileDevice {

	public:
		MobileDevice(std::string, bool connect);
		MobileDevice(std::string, int socket);
		~MobileDevice() { close_device(); };

		bool operator ==(const MobileDevice &dev);
		void close_device();
		void set_ip(std::string ip) { this->ip = ip; };
		void set_socket(int socket_fd) { this->socket_fd = socket_fd; };
		int get_socket() { return socket_fd; };
		bool establish_connection();
		int open_connection();
		bool send_message(const vatest_proto::ServerMessage&);
		bool receive_message(vatest_proto::ClientMessage&);
		void start_receiver();
		void stop_receiver();
		long receive_video(vanet::video *vid, const vatest_proto::ClientMessage &msg);
		char* receive_frame(const vatest_proto::ClientMessage &msg);
		cv::Mat *receive_frame_mat(const vatest_proto::ClientMessage &msg, int batch);
		uint32_t read_varint();
		void flush_socket();

	private:
		long mac_addr;
		std::string ip;
		int socket_fd;
		bool status;
		std::mutex lck_stop;
		std::thread receiver;
		void receiver_func();
		bool receiver_shutdown, receiver_running;
		std::condition_variable cond_stop;
	};
}

#endif
