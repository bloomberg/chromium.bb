#include "StdAfx.h"
#include "debug_server.h"

namespace {
const int kReadWaitMs = 20;
const int kReadBufferSize = 2048;
}  // namespace

namespace debug {
namespace rsp {
Server::Server()
    : host_is_connected_(false) {
}

Server::~Server() {
  Stop();
}

bool Server::Start(int listen_port,
                   const std::string& cmd,
                   const std::string& work_dir,
                   std::string* error) {
  if (!listening_socket_.Setup(listen_port)) {
    *error = "listening_socket_.Setup failed";
    return false;
  }

  if (!debug_core_.StartProcess(cmd.c_str(), work_dir.c_str(), error)) {
    Stop();
    return false;
  }
  return true;
}

void Server::DoWork() {
  if (!host_is_connected_) {
    if (listening_socket_.Accept(&host_connection_, 20)) {
      host_is_connected_ = true;
      OnHostConnected();
    }
  } else if (!host_connection_.IsConnected()) {
    host_is_connected_ = false;
    OnHostDisconnected();
  }
  debug_core_.DoWork(0);  //TODO: 0? //shall return : more work to do? + another func?(more work to do)
  if (host_connection_.IsConnected()) {
    
    char buff[kReadBufferSize];
    size_t read_bytes = host_connection_.Read(buff,
                                          sizeof(buff) - 1,
                                            kReadWaitMs);  // ?kReadWaitMs->0 ???
    if (read_bytes > 0) {
      buff[read_bytes] = 0;
      printf("h>%s\n", buff);
    }
  }
  //TODO: implement
}
void Server::Stop() {
  //TODO: implement
}
bool Server::IsRunning() const {
  //TODO: implement
  return true;
}

void Server::OnHostConnected() {
  //TODO: implement
}

void Server::OnHostDisconnected() {
  //TODO: implement
}

}  // namespace rsp
}  // namespace debug
