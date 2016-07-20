// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/remote_security_key_ipc_server.h"

#include <cstdint>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "remoting/base/logging.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/security_key/remote_security_key_ipc_server_impl.h"

namespace {

// Not thread safe, tests which set this value must do so on the same thread.
static remoting::RemoteSecurityKeyIpcServerFactory* g_factory = nullptr;

}  // namespace

namespace remoting {

void RemoteSecurityKeyIpcServer::SetFactoryForTest(
    RemoteSecurityKeyIpcServerFactory* factory) {
  g_factory = factory;
}

std::unique_ptr<RemoteSecurityKeyIpcServer> RemoteSecurityKeyIpcServer::Create(
    int connection_id,
    uint32_t peer_session_id,
    base::TimeDelta initial_connect_timeout,
    const GnubbyAuthHandler::SendMessageCallback& message_callback,
    const base::Closure& done_callback) {
  std::unique_ptr<RemoteSecurityKeyIpcServer> ipc_server =
      g_factory
          ? g_factory->Create(connection_id, peer_session_id,
                              initial_connect_timeout, message_callback,
                              done_callback)
          : base::WrapUnique(new RemoteSecurityKeyIpcServerImpl(
                connection_id, peer_session_id, initial_connect_timeout,
                message_callback, done_callback));

  return ipc_server;
}

}  // namespace remoting
