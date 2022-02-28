// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/fake_security_key_ipc_client.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ipc/ipc_channel.h"
#include "mojo/public/cpp/system/isolated_connection.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

FakeSecurityKeyIpcClient::FakeSecurityKeyIpcClient(
    const base::RepeatingClosure& channel_event_callback)
    : channel_event_callback_(channel_event_callback) {
  DCHECK(!channel_event_callback_.is_null());
}

FakeSecurityKeyIpcClient::~FakeSecurityKeyIpcClient() = default;

base::WeakPtr<FakeSecurityKeyIpcClient> FakeSecurityKeyIpcClient::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool FakeSecurityKeyIpcClient::CheckForSecurityKeyIpcServerChannel() {
  return check_for_ipc_channel_return_value_;
}

void FakeSecurityKeyIpcClient::EstablishIpcConnection(
    ConnectedCallback connected_callback,
    base::OnceClosure connection_error_callback) {
  if (establish_ipc_connection_should_succeed_) {
    std::move(connected_callback).Run();
  } else {
    std::move(connection_error_callback).Run();
  }
}

bool FakeSecurityKeyIpcClient::SendSecurityKeyRequest(
    const std::string& request_payload,
    ResponseCallback response_callback) {
  if (send_security_request_should_succeed_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(response_callback, security_key_response_payload_));
  }

  return send_security_request_should_succeed_;
}

void FakeSecurityKeyIpcClient::CloseIpcConnection() {
  client_channel_.reset();
  security_key_forwarder_.reset();
  mojo_connection_.reset();
  channel_event_callback_.Run();
}

bool FakeSecurityKeyIpcClient::ConnectViaIpc(
    const mojo::NamedPlatformChannel::ServerName& server_name) {
  mojo::PlatformChannelEndpoint endpoint =
      mojo::NamedPlatformChannel::ConnectToServer(server_name);
  if (!endpoint.is_valid())
    return false;

  mojo_connection_ = std::make_unique<mojo::IsolatedConnection>();
  client_channel_ = IPC::Channel::CreateClient(
      mojo_connection_->Connect(std::move(endpoint)).release(), this,
      base::ThreadTaskRunnerHandle::Get());
  if (!client_channel_->Connect()) {
    ADD_FAILURE() << "Failed to connect to the IPC channel.";
    return false;
  }

  auto* associated_interface_support =
      client_channel_->GetAssociatedInterfaceSupport();
  if (!associated_interface_support) {
    ADD_FAILURE() << "Failed to retrieve associated interface support.";
    return false;
  }

  associated_interface_support->GetRemoteAssociatedInterface(
      security_key_forwarder_.BindNewEndpointAndPassReceiver());

  return true;
}

void FakeSecurityKeyIpcClient::SendSecurityKeyRequestViaIpc(
    const std::string& request_payload) {
  security_key_forwarder_->OnSecurityKeyRequest(
      request_payload,
      base::BindOnce(&FakeSecurityKeyIpcClient::OnSecurityKeyResponse,
                     base::Unretained(this)));
}

bool FakeSecurityKeyIpcClient::OnMessageReceived(const IPC::Message& message) {
  ADD_FAILURE() << "Unexpected call to OnMessageReceived()";
  return false;
}

void FakeSecurityKeyIpcClient::OnChannelConnected(int32_t peer_pid) {
  ipc_channel_connected_ = true;
  connection_ready_ = true;
  channel_event_callback_.Run();
}

void FakeSecurityKeyIpcClient::OnChannelError() {
  ipc_channel_connected_ = false;
  channel_event_callback_.Run();
}

void FakeSecurityKeyIpcClient::OnSecurityKeyResponse(
    const std::string& request_data) {
  last_message_received_ = request_data;
  channel_event_callback_.Run();
}

}  // namespace remoting
