// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/fake_remote_security_key_ipc_server.h"

#include <cstdint>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/security_key/gnubby_auth_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

FakeRemoteSecurityKeyIpcServer::FakeRemoteSecurityKeyIpcServer(
    int connection_id,
    uint32_t peer_session_id,
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
  ipc_channel_.reset();
  channel_closed_callback_.Run();
}

base::WeakPtr<FakeRemoteSecurityKeyIpcServer>
FakeRemoteSecurityKeyIpcServer::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool FakeRemoteSecurityKeyIpcServer::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(FakeRemoteSecurityKeyIpcServer, message)
    IPC_MESSAGE_HANDLER(ChromotingRemoteSecurityKeyToNetworkMsg_Request,
                        SendRequest)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  EXPECT_TRUE(handled);
  return handled;
}

void FakeRemoteSecurityKeyIpcServer::OnChannelConnected(int32_t peer_pid) {}

void FakeRemoteSecurityKeyIpcServer::OnChannelError() {}

bool FakeRemoteSecurityKeyIpcServer::CreateChannel(
    const std::string& channel_name,
    base::TimeDelta request_timeout) {
  channel_name_ = channel_name;

  ipc_channel_ =
      IPC::Channel::CreateNamedServer(IPC::ChannelHandle(channel_name_), this);
  EXPECT_NE(nullptr, ipc_channel_);
  return ipc_channel_->Connect();
}

bool FakeRemoteSecurityKeyIpcServer::SendResponse(
    const std::string& message_data) {
  last_message_received_ = message_data;

  // This class works in two modes: one in which the test wants the IPC channel
  // to be created and used for notification and the second mode where the test
  // wants to notified of a response via a callback.  If a callback is set then
  // we use it, otherwise we will use the IPC connection to send a message.
  if (!send_response_callback_.is_null()) {
    send_response_callback_.Run();
    return true;
  }

  return ipc_channel_->Send(
      new ChromotingNetworkToRemoteSecurityKeyMsg_Response(message_data));
}

FakeRemoteSecurityKeyIpcServerFactory::FakeRemoteSecurityKeyIpcServerFactory() {
  RemoteSecurityKeyIpcServer::SetFactoryForTest(this);
}

FakeRemoteSecurityKeyIpcServerFactory::
    ~FakeRemoteSecurityKeyIpcServerFactory() {
  RemoteSecurityKeyIpcServer::SetFactoryForTest(nullptr);
}

std::unique_ptr<RemoteSecurityKeyIpcServer>
FakeRemoteSecurityKeyIpcServerFactory::Create(
    int connection_id,
    uint32_t peer_session_id,
    base::TimeDelta initial_connect_timeout,
    const GnubbyAuthHandler::SendMessageCallback& send_message_callback,
    const base::Closure& done_callback) {
  std::unique_ptr<FakeRemoteSecurityKeyIpcServer> fake_ipc_server(
      new FakeRemoteSecurityKeyIpcServer(connection_id, peer_session_id,
                                         initial_connect_timeout,
                                         send_message_callback, done_callback));

  ipc_server_map_[connection_id] = fake_ipc_server->AsWeakPtr();

  return base::WrapUnique(fake_ipc_server.release());
}

base::WeakPtr<FakeRemoteSecurityKeyIpcServer>
FakeRemoteSecurityKeyIpcServerFactory::GetIpcServerObject(int connection_id) {
  return ipc_server_map_[connection_id];
}

}  // namespace remoting
