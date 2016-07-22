#include <stdio.h>
#include <iostream>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <regex.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <google/protobuf/io/coded_stream.h>
#include <opencv2/highgui/highgui.hpp>
#include "mobile_device.h"
#include "classifier.h"
#include "test.h"
#include "vid_utils.h"

#include <iomanip>

using namespace std;
using namespace vatest_proto;

extern vanet::Classifier classifier;

namespace vanet {

	MobileDevice::MobileDevice(string ip, bool connect) {
		this->ip = ip;
		receiver_shutdown = false;
		if (connect) {
			if((socket_fd = open_connection()) < 0) {
				status = false;
			}
			status = true;
		}
		else
			status = false;
	}

	MobileDevice::MobileDevice(string ip, int socket) {
		this->ip = ip;
		this->socket_fd = socket;
		receiver_shutdown = false;
		status = false;
	}

	bool MobileDevice::operator ==(const MobileDevice &dev) {
		return ip == dev.ip && socket_fd == dev.socket_fd;
	}

	void MobileDevice::close_device() {
		if (status) {
			ServerMessage smsg;
			smsg.set_type(ServerMessage::DISCONNECT);
			send_message(smsg);
			close(socket_fd);
			status = false;
		}
	}

	bool MobileDevice::establish_connection() {
		ClientMessage c_msg;
		ServerMessage s_msg;
		if (!receive_message(c_msg) || c_msg.type() != ClientMessage::CONNECT)
			return false;
		s_msg.set_type(ServerMessage::CONNECT);
		if (!send_message(s_msg))
			return false;

		status = true;
		return true;
	}

	int MobileDevice::open_connection() {
		int client;
		struct sockaddr_in caddr;
		struct timeval tv_rcv, tv_snd;
		
		caddr.sin_family = AF_INET;
		caddr.sin_port = htons(DEFAULT_PORT);
		if (inet_aton(ip.c_str(), &caddr.sin_addr) == 0)
			return -1;
		client = socket(PF_INET, SOCK_STREAM, 0);
		if (client < 0) {
			cout << "Error on socket creation\n";
			return -1;
		}

		tv_rcv.tv_sec = RCV_TIMEOUT;
		tv_rcv.tv_usec = 0;
		tv_snd.tv_sec = SND_TIMEOUT;
		tv_snd.tv_usec = 0;

		setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &tv_rcv, sizeof(tv_rcv));
		setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, &tv_snd, sizeof(tv_snd));

		if (connect(client, (const struct sockaddr *)&caddr, sizeof(struct sockaddr)) < 0) {
			return -1;
		}
		cout << "Connected to " << ip << endl;
		return client;
	}


	bool MobileDevice::send_message(const ServerMessage& message) {
		google::protobuf::uint32 message_length = message.ByteSize();
		int tmp = message_length, varint_size = 1;
		while (tmp > 127) {
			varint_size++;
			tmp >>= 7;
		}

		int buffer_length = message_length + varint_size;
		google::protobuf::uint8 buffer[buffer_length];
		
		google::protobuf::io::ArrayOutputStream array_output(buffer, buffer_length);
		google::protobuf::io::CodedOutputStream coded_output(&array_output);

		coded_output.WriteVarint32(message_length);
		
		int size;
		int *val;

		coded_output.GetDirectBufferPointer((void **) &val, &size);
		

		if(!message.SerializeToCodedStream(&coded_output))
			return false;
		
		if (write(socket_fd, buffer, buffer_length) != buffer_length)
			return false;

		return true;
	}

	bool MobileDevice::receive_message(ClientMessage &message) {
		char *buf;
		int rb, readBytes = 0, size;

		size = read_varint();

		if (size == 0)
			return false;

		buf = new char[size];

		/*while (readBytes < size) {
			if ((rb = read(socket, buf + readBytes, size)) < 0)
				return false;
			if (rb == 0 && readBytes < size)
				return false;
			readBytes += rb;
		}*/

		if (recv(socket_fd, buf, size, MSG_WAITALL) != size)
			return false;

		google::protobuf::io::ArrayInputStream ais(buf, size);
		google::protobuf::io::CodedInputStream coded_input(&ais);

		if (!message.ParseFromCodedStream(&coded_input))
			return false;
		
		delete [] buf;

		return true;
	}

	uint32_t MobileDevice::read_varint() {
		int readBytes, rb, length = 0;
		char bite = 0;
		if ((rb = read(socket_fd, &bite, 1)) < 0)
			return 0;
		readBytes = rb;
		length = bite & 0x7f;
		while (bite & 0x80) {
			memset(&bite, 0, 1);
			if ((rb = read(socket_fd, &bite, 1)) < 0)
				return 0;
			readBytes += rb;
			length |= (bite & 0x7f) << (7*(readBytes - 1));
		}

		return length;
	}

	void MobileDevice::start_receiver() {
		receiver_running = true;
		receiver_shutdown = false;
		receiver = thread(&MobileDevice::receiver_func, this);
		receiver.join();
		receiver_running = false;
		receiver_shutdown = false;
	}


	void MobileDevice::stop_receiver() {
		unique_lock<mutex> lck(lck_stop);
		while (receiver_running)
			cond_stop.wait(lck);
		receiver_shutdown = true;
		lck.unlock();
	}

	void recv_frame(const ClientMessage& msg, int socket_fd) {
		FILE *file = fopen("test.jpg", "w");
		char *buf;
		int size = msg.size(0);

		buf = new char[size];

		if (recv(socket_fd, buf, size, MSG_WAITALL) != size)
			return;
		
		fwrite(buf, 1, size, file);
		fclose(file);
	}

	void MobileDevice::receiver_func() {
		int batch_size;
		
		while (status && !receiver_shutdown) {
			ClientMessage c_msg;
			if(!receive_message(c_msg))
				continue;
			if (c_msg.type() == ClientMessage::UPLINK_TEST) {
				cout << "Uplink test\n";
				long size = c_msg.size(0);
				char *buf = new char[size];
				recv(socket_fd, buf, size, MSG_WAITALL);
				delete[] buf;
			}
			if (c_msg.type() == ClientMessage::BATCH) {
					video *vid = new video;
					queue_item *item = new queue_item;
					vid->path = c_msg.path();
					item->dev = this;
					item->vid = vid;
					item->entire_video = false;
					item->batch = c_msg.batch();
					item->topK = c_msg.top_k();
					cout << "BATCH video: " << vid->path << ", size: " << c_msg.size(0) << endl;
				for (int i = 0; i < c_msg.size(0); i++) {
					ClientMessage msg;
					if (!receive_message(msg) || msg.type() != ClientMessage::FRAME)
						continue;
					cv::Mat *mat = receive_frame_mat(msg, item->batch);
					cout << "FRAME " << i + 1 << endl;
					item->mats.push_back(mat);
				}
				classifier.put(item);
			}
			else if (c_msg.type() == ClientMessage::VIDEO) {
				string path = VIDEO_PATH;
				video *vid = new video;
				vid->path = c_msg.path();
				if (receive_video(vid, c_msg) <= 0)
					continue;
				cout << "Received video: " << vid->path << ", topK=" << c_msg.top_k() << endl;
				classifier.extract(vid, c_msg.top_k(), this);
			}
			else if (c_msg.type() == ClientMessage::DISCONNECT) {
				cout << "Received disconnect\n";
				queue_item *item = new queue_item;
				item->vid = NULL;
				item->dev = this;
				item->last = true;
				classifier.put(item);
				break;
			}
		}
		cout << "Receiver shutdown\n";
		receiver_running = true;
		receiver_shutdown = false;
		cond_stop.notify_all();
	}

	void MobileDevice::flush_socket() {
		uint8_t *buffer = new uint8_t[DOWNLOAD_BUFFER_SIZE];

		while(read(socket_fd, buffer, DOWNLOAD_BUFFER_SIZE)> 0);
		
		delete [] buffer;
		// Add failed server message
	}

	char* MobileDevice::receive_frame(const ClientMessage &msg) {
		char *buf;
		int size = msg.size(0);

		buf = new char[size];

		if (recv(socket_fd, buf, size, MSG_WAITALL) != size)
			return NULL;

		return buf;
	}


	cv::Mat *MobileDevice::receive_frame_mat(const ClientMessage &msg, int batch) {
		char *buf;
		int size = msg.size(0);
		cout << "Frame size: " << size << endl;

		buf = new char[size];

		if (recv(socket_fd, buf, size, MSG_WAITALL) != size)
			return new cv::Mat();;
//changed March 3rd, 2016 by zongqing
//		if (msg.has_received_response() && msg.received_response() == true) {
//			ServerMessage smsg;
//			smsg.set_type(ServerMessage::RECEIVED);
//			smsg.set_batch(batch);
//			send_message(smsg);
//			cout << "RESPONSE SENT\n";
//		}
///////////////////////////
		cv::Mat raw = cv::Mat(1, size, CV_8UC1, buf);
		cv::Mat decoded = cv::imdecode(raw, cv::IMREAD_COLOR);
		cv::Mat *mat = new cv::Mat(decoded);

		return mat;;
	}


	// ADD FILE SUFFIX FOR REPEATS
	long MobileDevice::receive_video(video *vid, const ClientMessage &msg) {
		long rb, readBytes = 0, i, bars, size_kb, size, read_amt;
		uint8_t buf[DOWNLOAD_BUFFER_SIZE];
		string path(VIDEO_PATH);
		int pos = msg.path().find_last_of('/') + 1;
		path += msg.path().substr(pos);
		vid->localPath = path;
		FILE *file = fopen(path.c_str(), "w");

		size = msg.size(0);
		size_kb = size / 1024;
		fflush(stdout);
		system("setterm -cursor off");
		while (readBytes < size) {
			read_amt = size - readBytes;
			rb = read_amt < DOWNLOAD_BUFFER_SIZE ? read_amt : DOWNLOAD_BUFFER_SIZE;
			if ((rb = read(socket_fd, buf, rb))  < 0) {
				printf("\nReceive failed : [%s]\n", strerror(errno));
				flush_socket();
				return -1;
			}
			if (rb == 0 && readBytes < size) {
				printf("\nWrong file size: [%s]\n", strerror(errno));
				flush_socket();
				return -1;
			}
			fwrite(buf, 1, rb, file);
			readBytes += rb;
			
			if (rb) {
				printf("\r[");
				bars = readBytes * 50 / size;
				for(i = 0; i < 50; i++) {
					if (i < bars)
						printf("=");
					else
						printf(" ");
				}
				printf("] %ld/%ld KB", readBytes / 1024, size_kb);
				fflush(stdout);
			}
		}
		printf("\n");
		system("setterm -cursor on");

		//printf("File received successfully\n");

		fclose(file);
		return readBytes;
	}


}
