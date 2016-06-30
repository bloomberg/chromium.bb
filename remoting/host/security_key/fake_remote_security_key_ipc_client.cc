// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/fake_remote_security_key_ipc_client.h"

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "remoting/host/chromoting_messages.h"

namespace remoting {

FakeRemoteSecurityKeyIpcClient::FakeRemoteSecurityKeyIpcClient(
    const base::Closure& channel_event_callback)
    : channel_event_callback_(channel_event_callback), weak_factory_(this) {
  DCHECK(!channel_event_callback_.is_null());
}

FakeRemoteSecurityKeyIpcClient::~FakeRemoteSecurityKeyIpcClient() {}

base::WeakPtr<FakeRemoteSecurityKeyIpcClient>
FakeRemoteSecurityKeyIpcClient::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool FakeRemoteSecurityKeyIpcClient::WaitForSecurityKeyIpcServerChannel() {
  return wait_for_ipc_channel_return_value_;
}

void FakeRemoteSecurityKeyIpcClient::EstablishIpcConnection(
    const base::Closure& connection_ready_callback,
    const base::Closure& connection_error_callback) {
  if (establish_ipc_connection_should_succeed_) {
    connection_ready_callback.Run();
  } else {
    connection_error_callback.Run();
  }
}

bool FakeRemoteSecurityKeyIpcClient::SendSecurityKeyRequest(
    const std::string& request_payload,
    const ResponseCallback& response_callback) {
  if (send_security_request_should_succeed_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(response_callback, security_key_response_payload_));
  }

  return send_security_request_should_succeed_;
}

void FakeRemoteSecurityKeyIpcClient::CloseIpcConnection() {
  client_channel_.reset();
  channel_event_callback_.Run();
}

bool FakeRemoteSecurityKeyIpcClient::ConnectViaIpc(
    const std::string& channel_name) {
  // The retry loop is needed as the IPC Servers we connect to are reset (torn
  // down and recreated) in some tests and we should be resilient in that case.
  IPC::ChannelHandle channel_handle(channel_name);
  for (int i = 0; i < 5; i++) {
    client_channel_ = IPC::Channel::CreateNamedClient(channel_handle, this);
    if (client_channel_->Connect()) {
      return true;
    }

    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(),
        base::TimeDelta::FromMilliseconds(100));
    run_loop.Run();
  }

  return false;
}

void FakeRemoteSecurityKeyIpcClient::SendSecurityKeyRequestViaIpc(
    const std::string& request_payload) {
  client_channel_->Send(
      new ChromotingRemoteSecurityKeyToNetworkMsg_Request(request_payload));
}

bool FakeRemoteSecurityKeyIpcClient::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(FakeRemoteSecurityKeyIpcClient, message)
    IPC_MESSAGE_HANDLER(
        ChromotingNetworkToRemoteSecurityKeyMsg_ConnectionDetails,
        OnConnectionDetails)
    IPC_MESSAGE_HANDLER(ChromotingNetworkToRemoteSecurityKeyMsg_Response,
                        OnSecurityKeyResponse)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  CHECK(handled) << "Received unexpected IPC type: " << message.type();
  return handled;
}

void FakeRemoteSecurityKeyIpcClient::OnChannelConnected(int32_t peer_pid) {
  ipc_channel_connected_ = true;
  channel_event_callback_.Run();
}

void FakeRemoteSecurityKeyIpcClient::OnChannelError() {
  ipc_channel_connected_ = false;
  channel_event_callback_.Run();
}

void FakeRemoteSecurityKeyIpcClient::OnConnectionDetails(
    const std::string& channel_name) {
  last_message_received_ = channel_name;
  channel_event_callback_.Run();
}

void FakeRemoteSecurityKeyIpcClient::OnSecurityKeyResponse(
    const std::string& request_data) {
  last_message_received_ = request_data;
  channel_event_callback_.Run();
}

}  // namespace remoting
