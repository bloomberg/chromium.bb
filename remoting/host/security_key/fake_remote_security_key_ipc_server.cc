// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/fake_remote_security_key_ipc_server.h"

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_message.h"
#include "remoting/host/security_key/gnubby_auth_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

FakeRemoteSecurityKeyIpcServer::FakeRemoteSecurityKeyIpcServer(
    int connection_id,
    base::TimeDelta initial_connect_timeout,
    const GnubbyAuthHandler::SendMessageCallback& send_message_callback,
    const base::Closure& channel_closed_callback)
    : connection_id_(connection_id),
      send_message_callback_(send_message_callback),
      channel_closed_callback_(channel_closed_callback),
      weak_factory_(this) {}

FakeRemoteSecurityKeyIpcServer::~FakeRemoteSecurityKeyIpcServer() {}

void FakeRemoteSecurityKeyIpcServer::SendRequest(
    const std::string& message_data) {
  send_message_callback_.Run(connection_id_, message_data);
}

void FakeRemoteSecurityKeyIpcServer::CloseChannel() {
  channel_closed_callback_.Run();
}

base::WeakPtr<FakeRemoteSecurityKeyIpcServer>
FakeRemoteSecurityKeyIpcServer::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool FakeRemoteSecurityKeyIpcServer::OnMessageReceived(
    const IPC::Message& message) {
  return false;
}

void FakeRemoteSecurityKeyIpcServer::OnChannelConnected(int32_t peer_pid) {}

void FakeRemoteSecurityKeyIpcServer::OnChannelError() {}

bool FakeRemoteSecurityKeyIpcServer::CreateChannel(
    const std::string& channel_name,
    base::TimeDelta request_timeout) {
  channel_name_ = channel_name;
  return true;
}

bool FakeRemoteSecurityKeyIpcServer::SendResponse(
    const std::string& message_data) {
  last_message_received_ = message_data;

  send_response_callback_.Run();

  return true;
}

FakeRemoteSecurityKeyIpcServerFactory::FakeRemoteSecurityKeyIpcServerFactory() {
  RemoteSecurityKeyIpcServer::SetFactoryForTest(this);
}

FakeRemoteSecurityKeyIpcServerFactory::
    ~FakeRemoteSecurityKeyIpcServerFactory() {
  RemoteSecurityKeyIpcServer::SetFactoryForTest(nullptr);
}

scoped_ptr<RemoteSecurityKeyIpcServer>
FakeRemoteSecurityKeyIpcServerFactory::Create(
    int connection_id,
    base::TimeDelta initial_connect_timeout,
    const GnubbyAuthHandler::SendMessageCallback& send_message_callback,
    const base::Closure& done_callback) {
  scoped_ptr<FakeRemoteSecurityKeyIpcServer> fake_ipc_server(
      new FakeRemoteSecurityKeyIpcServer(connection_id, initial_connect_timeout,
                                         send_message_callback, done_callback));

  ipc_server_map_[connection_id] = fake_ipc_server->AsWeakPtr();

  return make_scoped_ptr(fake_ipc_server.release());
}

base::WeakPtr<FakeRemoteSecurityKeyIpcServer>
FakeRemoteSecurityKeyIpcServerFactory::GetIpcServerObject(int connection_id) {
  return ipc_server_map_[connection_id];
}

}  // namespace remoting
