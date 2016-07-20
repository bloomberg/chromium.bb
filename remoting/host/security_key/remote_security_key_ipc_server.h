// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SECURITY_KEY_REMOTE_SECURITY_KEY_IPC_SERVER_H_
#define REMOTING_HOST_SECURITY_KEY_REMOTE_SECURITY_KEY_IPC_SERVER_H_

#include <cstdint>
#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/time/time.h"
#include "remoting/host/security_key/gnubby_auth_handler.h"

namespace remoting {

class RemoteSecurityKeyIpcServerFactory;

// Responsible for handing the server end of the IPC channel between the
// network process (server) and the remote_security_key process (client).
class RemoteSecurityKeyIpcServer {
 public:
  virtual ~RemoteSecurityKeyIpcServer() {}

  // Creates a new RemoteSecurityKeyIpcServer instance.
  static std::unique_ptr<RemoteSecurityKeyIpcServer> Create(
      int connection_id,
      uint32_t peer_session_id,
      base::TimeDelta initial_connect_timeout,
      const GnubbyAuthHandler::SendMessageCallback& message_callback,
      const base::Closure& done_callback);

  // Used to set a Factory to generate fake/mock RemoteSecurityKeyIpcServer
  // instances for testing.
  static void SetFactoryForTest(RemoteSecurityKeyIpcServerFactory* factory);

  // Creates and starts listening on an IPC channel with the given name.
  virtual bool CreateChannel(const std::string& channel_name,
                             base::TimeDelta request_timeout) = 0;

  // Sends a security key response IPC message via the IPC channel.
  virtual bool SendResponse(const std::string& message_data) = 0;
};

// Used to allow for creating Fake/Mock RemoteSecurityKeyIpcServer for testing.
class RemoteSecurityKeyIpcServerFactory {
 public:
  virtual ~RemoteSecurityKeyIpcServerFactory() {}

  virtual std::unique_ptr<RemoteSecurityKeyIpcServer> Create(
      int connection_id,
      uint32_t peer_session_id,
      base::TimeDelta connect_timeout,
      const GnubbyAuthHandler::SendMessageCallback& message_callback,
      const base::Closure& done_callback) = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_SECURITY_KEY_REMOTE_SECURITY_KEY_IPC_SERVER_H_
