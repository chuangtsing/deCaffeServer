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
#include "vanet.h"
#include "vid_utils.h"
#include <regex.h>
#include <cstdarg>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <google/protobuf/io/coded_stream.h>


#include <iomanip>

using namespace std;

vector<pair<string, int>> clist;
vector<string> client_list;
int server = -1;

namespace vanet {

struct sockaddr_in ping_addr;
int ping_socket = -1;
string server_ip;

void vaout (const char* format, ...) {
	va_list args;
	string str(format);
	string space(OUTPUT_SPACE);
	size_t last, pos = 0;
	last = str.find_last_not_of("\n\r");
	while ((pos = str.find_first_of("\n\r", pos)) != string::npos) {
		if (last == string::npos || pos <= last) {
			str.insert(++pos, space);
			last += space.length();
		}
		else
			pos++;
	}
	str.insert(0, OUTPUT_SPACE);
	va_start (args, format);
	vprintf(str.c_str(), args);
	va_end(args);
}

int open_socket(const string &ip, int port, int rcv_timeout, int snd_timeout) {
	int client;
	struct sockaddr_in caddr;
	struct timeval tv_rcv, tv_snd;
	
	caddr.sin_family = AF_INET;
	caddr.sin_port = htons(port);
	if (inet_aton(ip.c_str(), &caddr.sin_addr) == 0)
		return -1;
	client = socket(PF_INET, SOCK_STREAM, 0);
	if (client < 0) {
		cout << "Error on socket creation\n";
		return -1;
	}

	tv_rcv.tv_sec = rcv_timeout;
	tv_rcv.tv_usec = 0;
	tv_snd.tv_sec = snd_timeout;
	tv_snd.tv_usec = 0;

	setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &tv_rcv, sizeof(tv_rcv));
	setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, &tv_snd, sizeof(tv_snd));

	if (connect(client, (const struct sockaddr *)&caddr, sizeof(struct sockaddr)) < 0) {
		return -1;
	}
	cout << "Connected to " << ip << endl;
	return client;
}

int open_ping_socket() {
	if (ping_socket != -1)
		return ping_socket;
	struct timeval tv_rcv, tv_snd;

	// Get server IP
	int sock;
	struct ifreq ifr;
	if((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		return -1;
	ifr.ifr_addr.sa_family = AF_INET;
	strncpy(ifr.ifr_name, "eth0", IFNAMSIZ - 1);
	if(ioctl(sock, SIOCGIFADDR, &ifr) < 0)
		return -1;
	/*if((ifr.ifr_flags & IFF_UP) && (ifr.ifr_flags & IFF_RUNNING)) {
		ioctl(sock, SIOCGIFADDR, &ifr);
		server_ip = inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);
	}
	else {
		close(sock);
		return -1;
	}*/

	server_ip = inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);
	close(sock);

	ping_addr.sin_family = AF_INET;
	ping_addr.sin_port = htons(DEFAULT_PING_PORT);
	
	int i = server_ip.find_last_of('.');
	string addr = server_ip.substr(0, i + 1) + "255";

	if (inet_aton(addr.c_str(), &ping_addr.sin_addr) == 0)
		return -1;
	ping_socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (ping_socket < 0) {
		cout << "Error on ping socket creation\n";
		return -1;
	}

	tv_snd.tv_sec = 5;
	tv_snd.tv_usec = 0;
	if (setsockopt(ping_socket, SOL_SOCKET, SO_SNDTIMEO, &tv_snd, sizeof(tv_snd))) {
		return -1;
	}
	int bcast_en = 1;
	if (setsockopt(ping_socket, SOL_SOCKET, SO_BROADCAST, &bcast_en, sizeof(bcast_en))) {
		return -1;
	}


	if (connect(ping_socket, (const struct sockaddr *)&ping_addr, sizeof(struct sockaddr)) < 0) {
		return -1;
	}

	return ping_socket;
}

void close_ping_socket() {
	if (ping_socket == -1)
		return;

	close(ping_socket);
	ping_socket = -1;
}

bool send_ping() {
	if (ping_socket == -1 && open_ping_socket() < 0)
		return false;
	vanet_pb::ServerMessage message;
	message.set_type(vanet_pb::ServerMessage::PING);
	message.set_ip(server_ip);
	uint32_t message_length = message.ByteSize();
	uint32_t send_length  = htonl(message.ByteSize());
	
	if (sendto(ping_socket, &send_length, sizeof(send_length), 0, (struct sockaddr *)&ping_addr, sizeof(ping_addr)) != sizeof(send_length))
		return false;

	google::protobuf::uint8 buffer[message_length];
	
	google::protobuf::io::ArrayOutputStream array_output(buffer, message_length);
	google::protobuf::io::CodedOutputStream coded_output(&array_output);

	int size;
	int *val;
	coded_output.GetDirectBufferPointer((void **) &val, &size);
	

	if(!message.SerializeToCodedStream(&coded_output))
		return false;
	
	if (sendto(ping_socket, buffer, message_length, 0, (struct sockaddr *)&ping_addr, sizeof(ping_addr)) != message_length)
		return false;

	return true;

}

bool discover_ping() {
	int optval;
	struct sockaddr_in saddr;
	fd_set master, read_fds, except_fds;
	FD_ZERO(&master);
	FD_ZERO(&read_fds);
	FD_ZERO(&except_fds);

	client_list.clear();
	
	if(!(open_ping_socket() >= 0 && send_ping()))
		return false;
	close_ping_socket();

	if ((server = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return false;
	
	optval = 1;
	if(setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) != 0)
		return false;

	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(DEFAULT_PORT);
	saddr.sin_addr.s_addr = htonl(INADDR_ANY);
	memset(&(saddr.sin_zero), 0, sizeof(saddr.sin_zero));

	if (bind(server, (struct sockaddr*)&saddr, sizeof(struct sockaddr)) == -1) {
		close(server);
		return false;
	}

	if (listen(server, 20) == -1) {
		close(server);
		return false;
	}

	FD_SET(server, &master);
	int fdmax = server;
	struct timeval tv_to;
	tv_to.tv_sec = 5;
	tv_to.tv_usec = 0;

	while (true) {
		read_fds = master;
		except_fds = master;
		int ret;
		if ((ret = select(fdmax + 1, &read_fds, NULL, &except_fds, &tv_to)) < 0)
			return false;
		else if (ret == 0)
			break;

		for (int i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &read_fds)) {
				if (i == server) {
					struct sockaddr_in caddr;
					unsigned int caddr_size = sizeof(caddr);
					int client = accept(server, (struct sockaddr *)&caddr, &caddr_size);
					clist.push_back(pair<string,int>(inet_ntoa((caddr.sin_addr)), client));
				}
			}
		}
	}

	close(server);

	return true;
}

/*int socket_read(int socket, void *buf, int length) {
	uint32_t rb, readBytes = 0;

    while (readBytes < length) {
    	if ((rb = read(socket, &buf[readBytes], length-readBytes))  < 0) {
    		printf("Receive failed : [%s]", strerror(errno) );
			return readBytes;
    	}
    	if (rb == 0) {
    		printf("Client socket closed on receive : [%s]", strerror(errno) );
			return readBytes;
    	}
        readBytes += rb;
    }

    return readBytes;
}

int socket_write(int socket, void *buf, int length) {
	// Local variables
    int sentBytes = 0, sb;

    // Loop until you have read all the bytes
    while (sentBytes < length) {
		// Read the bytes and check for error
		if ( (sb = write(socket, &buf[sentBytes], length-sentBytes)) < 0 ) {
			printf("Send failed : [%s]", strerror(errno) );
			return( -1 );
		}

		// Check for closed file
		else if (sb == 0) {
			// Close file, not an error
			printf("Client socket closed on send : [%s]", strerror(errno) );
			return( -1 );
		}

		// Now process what we read
		sentBytes += sb;
    }

    // Return successfully
    return sentBytes;
}*/


bool send_message(int socket, const vanet_pb::ServerMessage& message) {
	//google::protobuf::io::ZeroCopyOutputStream * rawOut
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
	
	if (write(socket, buffer, buffer_length) != buffer_length)
		return false;

	return true;
}

bool receive_message(int socket, vanet_pb::ClientMessage &message) {
	char *buf;
	int rb, readBytes = 0, size;

	if ((size = read_varint(socket)) == 0)
		return false;
	//google::protobuf::uint32 size;
	//coded_input.ReadVarint32(&size);

	buf = new char[size];

	while (readBytes < size) {
		if ((rb = read(socket, buf + readBytes, size)) < 0)
			return false;
		if (rb == 0 && readBytes < size)
			return false;
		readBytes += rb;
	}

	google::protobuf::io::ArrayInputStream ais(buf, size);
	google::protobuf::io::CodedInputStream coded_input(&ais);

	if (!message.ParseFromCodedStream(&coded_input))
		return false;
	
	delete [] buf;

	return true;
}

uint32_t read_varint(int socket) {
	int readBytes, rb, length = 0;
	char bite;
	if ((rb = read(socket, &bite, 1)) < 0)
		return 0;
	readBytes = rb;
	length = bite & 0x7f;
	while (bite & 0x80) {
		memset(&bite, 0, 1);
		if ((rb = read(socket, &bite, 1)) < 0)
			return 0;
		readBytes += rb;
		length |= (bite & 0x7f) << (7*(readBytes - 1));
	}

	return length;
}

int test_speed(int socket) {
	vanet_pb::ServerMessage s_message;
	s_message.set_type(vanet_pb::ServerMessage::SPEEDTEST);
	
	return 0;
}

void flush_socket(int socket) {
	uint8_t *buffer = new uint8_t[DOWNLOAD_BUFFER_SIZE];

	while(read(socket, buffer, DOWNLOAD_BUFFER_SIZE)> 0);
	
	delete [] buffer;
	// Add failed server message
}


// ADD FILE SUFFIX FOR REPEATS
long receive_video(int socket, string &path, const vanet_pb::ClientMessage &msg) {
	long rb, readBytes = 0, i, bars, size_kb, size, read_amt;
	uint8_t buf[DOWNLOAD_BUFFER_SIZE];
	path += msg.video_info(0).name();
	FILE *file = fopen(path.c_str(), "w");

	size = msg.video_info(0).size();
	size_kb = size / 1024;
	fflush(stdout);
	system("setterm -cursor off");
    while (readBytes < size) {
		read_amt = size - readBytes;
		rb = read_amt < DOWNLOAD_BUFFER_SIZE ? read_amt : DOWNLOAD_BUFFER_SIZE;
    	if ((rb = read(socket, buf, rb))  < 0) {
    		printf("\nReceive failed : [%s]\n", strerror(errno));
			flush_socket(socket);
			return -1;
    	}
    	if (rb == 0 && readBytes < size) {
    		printf("\nWrong file size: [%s]\n", strerror(errno));
			flush_socket(socket);
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

bool send_resources(int socket, const google::protobuf::RepeatedField<int>& res) {
	struct stat f;
	vanet_pb::ServerMessage msg;
	vector<pair<string, uint64_t>> files;

	cout << "Sending resources\n";

	msg.set_type(vanet_pb::ServerMessage::RES);
	for (auto r : res) {
		string path;
		if (r == vanet_pb::Resource::MODEL) {
			path = MODEL_PATH;
		}
		else if (r == vanet_pb::Resource::WEIGHTS) {
			path = WEIGHT_PATH;
		}
		else if (r == vanet_pb::Resource::MEAN) {
			path = MEAN_PATH;
		}
		else if (r == vanet_pb::Resource::SYNSET) {
			path = SYNSET_PATH;
		}


		if (stat(path.c_str(), &f))
			return false;
		
		msg.add_resources((vanet_pb::Resource) r);
		long size = f.st_size;
		msg.add_size(size);
		files.push_back(pair<string, long>(path, size));
		cout << "Added " << path << " (" << size << "B)\n";
	}


	if (!send_message(socket, msg))
		return false;
	
	for (auto f : files) {
		int fd = open(f.first.c_str(), O_RDONLY);
		cout << "Sending file\n";
		if (!send_file(socket, fd, f.second))
			return false;
	}	
	return true;
}

bool send_file(int socket, int fd, long size) {
	long sb, sentBytes = 0, i, bars, size_kb, send_amt;
	uint8_t buf[DOWNLOAD_BUFFER_SIZE];

	size_kb = size / 1024;
	fflush(stdout);
	system("setterm -cursor off");
	send_amt = size;
	sb = send_amt < DOWNLOAD_BUFFER_SIZE ? send_amt : DOWNLOAD_BUFFER_SIZE;
    while (send_amt) {
    	if ((sb = read(fd, buf, sb))  < 0) {
    		printf("\nSend failed: [%s]\n", strerror(errno));
			flush_socket(socket);
			close(fd);
			return false;
    	}
		if(write(socket, buf, sb) != sb) {
    		printf("\nSend failed: [%s]\n", strerror(errno));
			flush_socket(socket);
			close(fd);
			return false;
    	}
        sentBytes += sb;
        
        if (sb) {
        	printf("\r[");
        	bars = sentBytes * 50 / size;
        	for(i = 0; i < 50; i++) {
        		if (i < bars)
        			printf("=");
        		else
        			printf(" ");
        	}
        	printf("] %ld/%ld KB", sentBytes , size);
        	fflush(stdout);
    	}
		send_amt = size - sentBytes;
		sb = send_amt < DOWNLOAD_BUFFER_SIZE ? send_amt : DOWNLOAD_BUFFER_SIZE;
    }
    printf("\n");
	system("setterm -cursor on");

    //printf("File received successfully\n");

    close(fd);
    return true;
}

/*bool discover_clients_multicast() {
	char line[1024], **list, ip[16];
	int begin, end;
	string ip_str;
	vector<string> discovered;
	
	cout << "Discovering clients on network...\n";
	sprintf(line, "nmap %s %s/24 2>&1", NMAP_ARGS, gateway.c_str());
	output = popen(line, "r");
	if (!output) {
		cout << "Nmap error\n";
		return false;
	}

	if (regcomp(&reg, REGEX_IP, REG_EXTENDED)) {
		return false;
	}
        
	while (fgets(line, sizeof(line) - 1, output)) {
		if (!regexec(&reg, line, 1, &match, 0)) {
			begin = (int)match.rm_so;
			end = (int)match.rm_eo;
			if (end - begin)
				memcpy(ip, &line[begin], end-begin);
			ip[end-begin] = '\0';
			ip_str = string(ip);
			if (ip_str != gateway && ip_str != "10.100.1.100") {
				cout << ip << endl;
				discovered.push_back(string(ip));
			}
		}
	}

	client_list = discovered;
	regfree(&reg);

	
	cout << (int)client_list.size() << " clients found\n";

	return true;
}*/

bool discover_clients() {
	char line[1024], ip[16];
	FILE *output;
	regex_t reg;
	regmatch_t match;
	int begin, end;
	string ip_str;
	vector<string> discovered;
	
	cout << "Discovering clients on network...\n";
	sprintf(line, "nmap %s %s/24 2>&1", NMAP_ARGS, gateway.c_str());
	output = popen(line, "r");
	if (!output) {
		cout << "Nmap error\n";
		return false;
	}

	if (regcomp(&reg, REGEX_IP, REG_EXTENDED)) {
		return false;
	}
        
	while (fgets(line, sizeof(line) - 1, output)) {
		if (!regexec(&reg, line, 1, &match, 0)) {
			begin = (int)match.rm_so;
			end = (int)match.rm_eo;
			if (end - begin)
				memcpy(ip, &line[begin], end-begin);
			ip[end-begin] = '\0';
			ip_str = string(ip);
			if (ip_str != gateway && ip_str != "10.100.1.100") {
				cout << ip << endl;
				discovered.push_back(string(ip));
			}
		}
	}

	client_list = discovered;
	regfree(&reg);

	
	cout << (int)client_list.size() << " clients found\n";

	return true;
}

/*int add_host_node(host_node_t **head, char *ip) {
	host_node_t *node;
	node = (host_node_t *) malloc(sizeof(host_node_t));
	strncpy(node->ip, ip, 16);
	if (!*head) {
		*head = node;
		node->next = NULL;
	}
	else {
		node->next = *head;
		*head = node;
	}
	return 0;
}

void free_host_list(host_node_t *node) {
	while (node) {
		free(node);
		node = node->next;
	}
}

void get_hosts(host_node_t *node) {
	while (node) {
		printf("%s\n", node->ip);
		node = node->next;
	}
}*/
}
