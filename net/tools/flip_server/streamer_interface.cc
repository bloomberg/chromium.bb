// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/flip_server/streamer_interface.h"

#include <string>

#include "net/tools/flip_server/constants.h"
#include "net/tools/flip_server/flip_config.h"
#include "net/tools/flip_server/sm_connection.h"

namespace net {

StreamerSM::StreamerSM(SMConnection* connection,
                       SMInterface* sm_other_interface,
                       EpollServer* epoll_server,
                       FlipAcceptor* acceptor)
    : connection_(connection),
      sm_other_interface_(sm_other_interface),
      epoll_server_(epoll_server),
      acceptor_(acceptor) {
  VLOG(2) << ACCEPTOR_CLIENT_IDENT << "Creating StreamerSM object";
}

StreamerSM::~StreamerSM() {
  VLOG(1) << ACCEPTOR_CLIENT_IDENT << "Destroying StreamerSM object";
  Reset();
}

void StreamerSM::InitSMInterface(SMInterface* sm_other_interface,
                                 int32 server_idx) {
  sm_other_interface_ = sm_other_interface;
}

void StreamerSM::InitSMConnection(SMConnectionPoolInterface* connection_pool,
                                  SMInterface* sm_interface,
                                  EpollServer* epoll_server,
                                  int fd,
                                  std::string server_ip,
                                  std::string server_port,
                                  std::string remote_ip,
                                  bool use_ssl) {
  VLOG(2) << ACCEPTOR_CLIENT_IDENT << "StreamerSM: Initializing server "
          << "connection.";
  connection_->InitSMConnection(connection_pool, sm_interface,
                                epoll_server, fd, server_ip,
                                server_port, remote_ip, use_ssl);
}

size_t StreamerSM::ProcessReadInput(const char* data, size_t len) {
  return sm_other_interface_->ProcessWriteInput(data, len);
}

size_t StreamerSM::ProcessWriteInput(const char* data, size_t len) {
  char * dataPtr = new char[len];
  memcpy(dataPtr, data, len);
  DataFrame* df = new DataFrame;
  df->data = (const char *)dataPtr;
  df->size = len;
  df->delete_when_done = true;
  connection_->EnqueueDataFrame(df);
  return len;
}

bool StreamerSM::MessageFullyRead() const {
  return false;
}

bool StreamerSM::Error() const {
  return false;
}

const char* StreamerSM::ErrorAsString() const {
  return "(none)";
}

void StreamerSM::Reset() {
  VLOG(1) << ACCEPTOR_CLIENT_IDENT << "StreamerSM: Reset";
  connection_->Cleanup("Server Reset");
}

void StreamerSM::ResetForNewConnection() {
  sm_other_interface_->Reset();
}

int StreamerSM::PostAcceptHook() {
  if (!sm_other_interface_) {
    SMConnection *server_connection =
      SMConnection::NewSMConnection(epoll_server_, NULL, NULL,
                                    acceptor_, "server_conn: ");
    if (server_connection == NULL) {
      LOG(ERROR) << "StreamerSM: Could not create server conenction.";
      return 0;
    }
    VLOG(2) << ACCEPTOR_CLIENT_IDENT << "StreamerSM: Creating new server "
            << "connection.";
    sm_other_interface_ = new StreamerSM(server_connection, this,
                                         epoll_server_, acceptor_);
    sm_other_interface_->InitSMInterface(this, 0);
  }
  // The Streamer interface is used to stream HTTPS connections, so we
  // will always use the https_server_ip/port here.
  sm_other_interface_->InitSMConnection(NULL, sm_other_interface_,
                                        epoll_server_, -1,
                                        acceptor_->https_server_ip_,
                                        acceptor_->https_server_port_,
                                        "",
                                        false);

  return 1;
}

size_t StreamerSM::SendSynStream(uint32 stream_id,
                                 const BalsaHeaders& headers) {
  return 0;
}

size_t StreamerSM::SendSynReply(uint32 stream_id, const BalsaHeaders& headers) {
  return 0;
}

}  // namespace net

