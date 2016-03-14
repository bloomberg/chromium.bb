// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SECURITY_KEY_FAKE_SECURITY_KEY_IPC_CLIENT_H_
#define REMOTING_HOST_SECURITY_KEY_FAKE_SECURITY_KEY_IPC_CLIENT_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "ipc/ipc_listener.h"

namespace IPC {
class Channel;
class Message;
}  // IPC

namespace remoting {

// Simulates the RemoteSecurityKeyIpcClient and provides access to data members
// for testing.
class FakeRemoteSecurityKeyIpcClient : public IPC::Listener {
 public:
  explicit FakeRemoteSecurityKeyIpcClient(base::Closure channel_event_callback);
  ~FakeRemoteSecurityKeyIpcClient() override;

  // Connects as a client to the |channel_name| IPC Channel.
  bool Connect(const std::string& channel_name);

  // Closes the |client_channel_| IPC channel.
  void CloseChannel();

  // Sends a security key request message via IPC through |client_channel_|.
  void SendRequest(const std::string& request_data);

  // Provides access to |last_message_received_| for testing.
  const std::string& last_message_received() const {
    return last_message_received_;
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
  scoped_ptr<IPC::Channel> client_channel_;

  // Provides the contents of the last IPC message received.
  std::string last_message_received_;

  DISALLOW_COPY_AND_ASSIGN(FakeRemoteSecurityKeyIpcClient);
};

}  // namespace remoting

#endif  // REMOTING_HOST_SECURITY_KEY_FAKE_SECURITY_KEY_IPC_CLIENT_H_