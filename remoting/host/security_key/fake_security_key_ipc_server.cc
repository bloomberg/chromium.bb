// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/fake_security_key_ipc_server.h"

#include <cstdint>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/named_platform_handle.h"
#include "mojo/edk/embedder/named_platform_handle_utils.h"
#include "mojo/edk/embedder/peer_connection.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/security_key/security_key_auth_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

FakeSecurityKeyIpcServer::FakeSecurityKeyIpcServer(
    int connection_id,
    ClientSessionDetails* client_session_details,
    base::TimeDelta initial_connect_timeout,
    const SecurityKeyAuthHandler::SendMessageCallback& send_message_callback,
    const base::Closure& connect_callback,
    const base::Closure& channel_closed_callback)
    : connection_id_(connection_id),
      send_message_callback_(send_message_callback),
      connect_callback_(connect_callback),
      channel_closed_callback_(channel_closed_callback),
      weak_factory_(this) {}

FakeSecurityKeyIpcServer::~FakeSecurityKeyIpcServer() = default;

void FakeSecurityKeyIpcServer::SendRequest(const std::string& message_data) {
  send_message_callback_.Run(connection_id_, message_data);
}

void FakeSecurityKeyIpcServer::CloseChannel() {
  ipc_channel_.reset();
  peer_connection_.reset();
  channel_closed_callback_.Run();
}

base::WeakPtr<FakeSecurityKeyIpcServer> FakeSecurityKeyIpcServer::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool FakeSecurityKeyIpcServer::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(FakeSecurityKeyIpcServer, message)
    IPC_MESSAGE_HANDLER(ChromotingRemoteSecurityKeyToNetworkMsg_Request,
                        SendRequest)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  EXPECT_TRUE(handled);
  return handled;
}

void FakeSecurityKeyIpcServer::OnChannelConnected(int32_t peer_pid) {
  connect_callback_.Run();
}

bool FakeSecurityKeyIpcServer::CreateChannel(
    const mojo::edk::NamedPlatformHandle& channel_handle,
    base::TimeDelta request_timeout) {
  mojo::edk::CreateServerHandleOptions options;
#if defined(OS_WIN)
  options.enforce_uniqueness = false;
#endif
  peer_connection_ = base::MakeUnique<mojo::edk::PeerConnection>();
  ipc_channel_ = IPC::Channel::CreateServer(
      peer_connection_
          ->Connect(mojo::edk::ConnectionParams(
              mojo::edk::TransportProtocol::kLegacy,
              mojo::edk::CreateServerHandle(channel_handle, options)))
          .release(),
      this, base::ThreadTaskRunnerHandle::Get());
  EXPECT_NE(nullptr, ipc_channel_);
  return ipc_channel_->Connect();
}

bool FakeSecurityKeyIpcServer::SendResponse(const std::string& message_data) {
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

void FakeSecurityKeyIpcServer::SendConnectionReadyMessage() {
  ipc_channel_->Send(
      new ChromotingNetworkToRemoteSecurityKeyMsg_ConnectionReady());
}

void FakeSecurityKeyIpcServer::SendInvalidSessionMessage() {
  ipc_channel_->Send(
      new ChromotingNetworkToRemoteSecurityKeyMsg_InvalidSession());
}

FakeSecurityKeyIpcServerFactory::FakeSecurityKeyIpcServerFactory() {
  SecurityKeyIpcServer::SetFactoryForTest(this);
}

FakeSecurityKeyIpcServerFactory::~FakeSecurityKeyIpcServerFactory() {
  SecurityKeyIpcServer::SetFactoryForTest(nullptr);
}

std::unique_ptr<SecurityKeyIpcServer> FakeSecurityKeyIpcServerFactory::Create(
    int connection_id,
    ClientSessionDetails* client_session_details,
    base::TimeDelta initial_connect_timeout,
    const SecurityKeyAuthHandler::SendMessageCallback& send_message_callback,
    const base::Closure& connect_callback,
    const base::Closure& done_callback) {
  auto fake_ipc_server = base::MakeUnique<FakeSecurityKeyIpcServer>(
      connection_id, client_session_details, initial_connect_timeout,
      send_message_callback, connect_callback, done_callback);

  ipc_server_map_[connection_id] = fake_ipc_server->AsWeakPtr();

  return std::move(fake_ipc_server);
}

base::WeakPtr<FakeSecurityKeyIpcServer>
FakeSecurityKeyIpcServerFactory::GetIpcServerObject(int connection_id) {
  return ipc_server_map_[connection_id];
}

}  // namespace remoting
