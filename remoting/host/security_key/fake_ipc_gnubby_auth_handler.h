// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SECURITY_KEY_FAKE_IPC_GNUBBY_AUTH_HANDLER_H_
#define REMOTING_HOST_SECURITY_KEY_FAKE_IPC_GNUBBY_AUTH_HANDLER_H_

#include "remoting/host/security_key/gnubby_auth_handler.h"

#include <memory>
#include <string>

#include "base/time/time.h"
#include "ipc/ipc_listener.h"

namespace IPC {
class Channel;
class Message;
}  // IPC

namespace remoting {

// Simplified implementation of an IPC based GnubbyAuthHandler class used for
// testing.
class FakeIpcGnubbyAuthHandler : public GnubbyAuthHandler,
                                 public IPC::Listener {
 public:
  FakeIpcGnubbyAuthHandler();
  ~FakeIpcGnubbyAuthHandler() override;

  // GnubbyAuthHandler interface.
  void CreateGnubbyConnection() override;
  bool IsValidConnectionId(int gnubby_connection_id) const override;
  void SendClientResponse(int gnubby_connection_id,
                          const std::string& response) override;
  void SendErrorAndCloseConnection(int gnubby_connection_id) override;
  void SetSendMessageCallback(const SendMessageCallback& callback) override;
  size_t GetActiveConnectionCountForTest() const override;
  void SetRequestTimeoutForTest(base::TimeDelta timeout) override;

  void set_ipc_security_key_channel_name(
      const std::string& ipc_security_key_channel_name) {
    ipc_security_key_channel_name_ = ipc_security_key_channel_name;
  }

  void set_ipc_server_channel_name(const std::string& ipc_server_channel_name) {
    ipc_server_channel_name_ = ipc_server_channel_name;
  }

 private:
  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnChannelConnected(int32_t peer_pid) override;
  void OnChannelError() override;

  // The name used when creating the well-known IPC Server channel.
  std::string ipc_server_channel_name_;

  // The channel name returned in the test connection details message and used
  // to exchange security key messages over IPC.
  std::string ipc_security_key_channel_name_;

  // IPC Clients connect to this channel first to receive their own unique IPC
  // channel to start a security key forwarding session on.
  std::unique_ptr<IPC::Channel> ipc_server_channel_;

  DISALLOW_COPY_AND_ASSIGN(FakeIpcGnubbyAuthHandler);
};

}  // namespace remoting

#endif  // REMOTING_HOST_SECURITY_KEY_FAKE_IPC_GNUBBY_AUTH_HANDLER_H_
