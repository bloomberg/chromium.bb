// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SECURITY_KEY_REMOTE_SECURITY_KEY_IPC_SERVER_IMPL_H_
#define REMOTING_HOST_SECURITY_KEY_REMOTE_SECURITY_KEY_IPC_SERVER_IMPL_H_

#include "remoting/host/security_key/remote_security_key_ipc_server.h"

#include <cstdint>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ipc/ipc_listener.h"

namespace base {
class TimeDelta;
}  // base

namespace IPC {
class Channel;
class Message;
}  // IPC

namespace remoting {

// Responsible for handing the server end of the IPC channel between the
// the network process and the local remote_security_key process.
class RemoteSecurityKeyIpcServerImpl : public RemoteSecurityKeyIpcServer,
                                       public IPC::Listener {
 public:
  RemoteSecurityKeyIpcServerImpl(
      int connection_id,
      uint32_t peer_session_id,
      base::TimeDelta initial_connect_timeout,
      const GnubbyAuthHandler::SendMessageCallback& message_callback,
      const base::Closure& done_callback);
  ~RemoteSecurityKeyIpcServerImpl() override;

  // RemoteSecurityKeyIpcServer implementation.
  bool CreateChannel(const std::string& channel_name,
                     base::TimeDelta request_timeout) override;
  bool SendResponse(const std::string& message_data) override;

 private:
  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnChannelConnected(int32_t peer_pid) override;
  void OnChannelError() override;

  // Handles security key resquest IPC messages.
  void OnSecurityKeyRequest(const std::string& request);

  // The value assigned to identify the current IPC channel.
  int connection_id_;

  // The expected session id of the process connecting to the IPC channel.
  uint32_t peer_session_id_;

  // Tracks whether the connection is in the process of being closed.
  bool connection_close_pending_ = false;

  // Timeout for disconnecting the IPC channel if there is no client activity.
  base::TimeDelta initial_connect_timeout_;

  // Timeout for disconnecting the IPC channel if there is no response from
  // the remote client after a security key request.
  base::TimeDelta security_key_request_timeout_;

  // Used to detect timeouts and disconnect the IPC channel.
  base::OneShotTimer timer_;

  // Used to signal that the IPC channel should be disconnected.
  base::Closure done_callback_;

  // Used to pass a security key request on to the remote client.
  GnubbyAuthHandler::SendMessageCallback message_callback_;

  // Used for sending/receiving security key messages between processes.
  std::unique_ptr<IPC::Channel> ipc_channel_;

  // Ensures RemoteSecurityKeyIpcServerImpl methods are called on the same
  // thread.
  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<RemoteSecurityKeyIpcServerImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RemoteSecurityKeyIpcServerImpl);
};

}  // namespace remoting

#endif  // REMOTING_HOST_SECURITY_KEY_REMOTE_SECURITY_KEY_IPC_SERVER_IMPL_H_
