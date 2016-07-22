#ifndef VANET_H
#define VANET_H

#include <string>
#include <vector>
#include <unistd.h>
#include "protobuf/vanet_pb.pb.h"
#include <chrono>

//#include <stdint.h>

//#define IS_BIG_ENDIAN (*(uint16_t *)"\0\xff" < 0x100)

#define IP_SIZE 16
#define CMD_SIZE 200
#define CMD_QUIT "q"
#define CMD_PULL_INFO "i"
#define CMD_SET_IP "s"
#define PULL_INFO "VASERVER_POLL"
#define DEFAULT_IP "192.168.1.10" //127.0.0.1
//#define DEFAULT_IP "10.100.1.101" //127.0.0.1
#define DEFAULT_PORT 38300
#define DEFAULT_PING_PORT 1900
#define DOWNLOAD_BUFFER_SIZE 65536
#define NMAP_ARGS "-sn"
#define DEFAULT_GATEWAY "192.168.1.1"
//#define DEFAULT_GATEWAY "10.100.1.1"
#define REGEX_IP "[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}"
//#define REGEX_IP "[0-9]+"
#define NO_REGEX "[0-9]+"
#define RCV_TIMEOUT 15
#define SND_TIMEOUT 15
#define PING_TIMEOUT 2

#define INPUT_PROMPT "VA_Server >> "
#define OUTPUT_SPACE "             "

//#define htonll(x) (IS_BIG_ENDIAN ? x : ((((uint64_t)htonl(x)) << 32) + htonl((x) >> 32)))

extern int port;
extern std::string gateway;

namespace vanet {

typedef struct video {
	std::string name;
	std::string timestamp;
	std::string path;
	int64_t size;
	int64_t duration;
	std::string mime;
	double loc_lat;
	double loc_long;
	int32_t width;
	int32_t height;
	int32_t rotation;
	std::vector<std::string> tags;
} video;

typedef enum task_type {
	INIT = 0,
	QUERY_ALL = 1,
	QUERY_TAGS = 2
} task_type;


typedef std::chrono::high_resolution_clock Time;
typedef std::chrono::milliseconds MS;
typedef std::chrono::duration<double> Duration;
typedef std::chrono::system_clock::time_point TimePoint;


void vaout(const char *, ...);
int open_socket(const std::string &ip, int port, int rcv_timeout, int snd_timeout);
int open_ping_socket();
void close_ping_socket();
bool send_ping();
bool discover_ping();
bool send_message(int, const vanet_pb::ServerMessage&);
bool receive_message(int, vanet_pb::ClientMessage&);
uint32_t read_varint(int);
bool send_resources(int socket, const google::protobuf::RepeatedField<int>& res);
bool send_file(int socket, int fd, long size);
//int socket_read(int, void*, int);
//int socket_read(int, void*, int);
long receive_video(int socket, std::string &path, const vanet_pb::ClientMessage &msg);
bool discover_clients();
//int add_host_node(host_node_t**, char*);
//void free_host_list(host_node_t*);
//void get_hosts(host_node_t*);
}

#endif
