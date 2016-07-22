#ifndef TASK_HANDLERS_HPP
#define TASK_HANDLERS_HPP

#include <string>
#include "protobuf/vanet_pb.pb.h"
#include <thread>

extern std::vector<std::tuple<std::string,int,vanet_pb::ClientMessage>> c_messages;

namespace vanet {
typedef enum Scheduling { OPT, MOBILE, SERVER } Scheduling;
void init_handler(int socket);
void query_all_handler(int socket, const std::string &ip);
void query_task(Scheduling sched);
void query_basic();
}

#endif
