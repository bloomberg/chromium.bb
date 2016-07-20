// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SECURITY_KEY_FAKE_SECURITY_KEY_IPC_CLIENT_H_
#define REMOTING_HOST_SECURITY_KEY_FAKE_SECURITY_KEY_IPC_CLIENT_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "remoting/host/security_key/remote_security_key_ipc_client.h"

namespace IPC {
class Channel;
class Message;
}  // IPC

namespace remoting {

// Simulates the RemoteSecurityKeyIpcClient and provides access to data members
// for testing.  This class is used for scenarios which require an IPC channel
// as well as for tests which only need callbacks activated.
class FakeRemoteSecurityKeyIpcClient : public RemoteSecurityKeyIpcClient {
 public:
  explicit FakeRemoteSecurityKeyIpcClient(
      const base::Closure& channel_event_callback);
  ~FakeRemoteSecurityKeyIpcClient() override;

  // RemoteSecurityKeyIpcClient interface.
  bool WaitForSecurityKeyIpcServerChannel() override;
  void EstablishIpcConnection(
      const base::Closure& connection_ready_callback,
      const base::Closure& connection_error_callback) override;
  bool SendSecurityKeyRequest(
      const std::string& request_payload,
      const ResponseCallback& response_callback) override;
  void CloseIpcConnection() override;

  // Connects as a client to the |channel_name| IPC Channel.
  bool ConnectViaIpc(const std::string& channel_name);

  // Override of SendSecurityKeyRequest() interface method for tests which use
  // an IPC channel for testing.
  void SendSecurityKeyRequestViaIpc(const std::string& request_payload);

  base::WeakPtr<FakeRemoteSecurityKeyIpcClient> AsWeakPtr();

  const std::string& last_message_received() const {
    return last_message_received_;
  }

  bool ipc_channel_connected() { return ipc_channel_connected_; }

  void set_wait_for_ipc_channel_return_value(bool return_value) {
    wait_for_ipc_channel_return_value_ = return_value;
  }

  void set_establish_ipc_connection_should_succeed(bool should_succeed) {
    establish_ipc_connection_should_succeed_ = should_succeed;
  }

  void set_send_security_request_should_succeed(bool should_succeed) {
    send_security_request_should_succeed_ = should_succeed;
  }

  void set_security_key_response_payload(const std::string& response_payload) {
    security_key_response_payload_ = response_payload;
  }

 private:
  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnChannelConnected(int32_t peer_pid) override;
  void OnChannelError() override;

  // Handles the initial IPC message used to establish a side channel with this
  // IPC Client instance.
  void OnConnectionDetails(const std::string& request_data);

  // Handles security key response IPC messages.
  void OnSecurityKeyResponse(const std::string& request_data);

  // Called when a change in the IPC channel state has occurred.
  base::Closure channel_event_callback_;

  // Used for sending/receiving security key messages between processes.
  std::unique_ptr<IPC::Channel> client_channel_;

  // Provides the contents of the last IPC message received.
  std::string last_message_received_;

  // Determines whether EstablishIpcConnection() returns success or failure.
  bool establish_ipc_connection_should_succeed_ = true;

  // Determines whether SendSecurityKeyRequest() returns success or failure.
  bool send_security_request_should_succeed_ = true;

  // Value returned by WaitForSecurityKeyIpcServerChannel() method.
  bool wait_for_ipc_channel_return_value_ = true;

  // Stores whether a connection to the server IPC channel is active.
  bool ipc_channel_connected_ = false;

  // Value returned by SendSecurityKeyRequest() method.
  std::string security_key_response_payload_;

  base::WeakPtrFactory<FakeRemoteSecurityKeyIpcClient> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeRemoteSecurityKeyIpcClient);
};

}  // namespace remoting

#endif  // REMOTING_HOST_SECURITY_KEY_FAKE_SECURITY_KEY_IPC_CLIENT_H_
