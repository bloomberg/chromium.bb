// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/test_tools/server_thread.h"

#include "net/tools/quic/test_tools/quic_server_peer.h"

namespace net {
namespace tools {
namespace test {

ServerThread::ServerThread(IPEndPoint address,
                           const QuicConfig& config,
                           const QuicVersionVector& supported_versions,
                           bool strike_register_no_startup_period)
    : SimpleThread("server_thread"),
      listening_(true, false),
      confirmed_(true, false),
      quit_(true, false),
      server_(config, supported_versions),
      address_(address),
      port_(0) {
  if (strike_register_no_startup_period) {
    server_.SetStrikeRegisterNoStartupPeriod();
  }
}

ServerThread::~ServerThread() {
}

void ServerThread::Run() {
  server_.Listen(address_);

  port_lock_.Acquire();
  port_ = server_.port();
  port_lock_.Release();

  listening_.Signal();
  while (!quit_.IsSignaled()) {
    event_loop_mu_.Acquire();
    server_.WaitForEvents();
    MaybeNotifyOfHandshakeConfirmation();
    event_loop_mu_.Release();
  }

  server_.Shutdown();
}

int ServerThread::GetPort() {
  port_lock_.Acquire();
  int rc = port_;
  port_lock_.Release();
    return rc;
}

void ServerThread::WaitForServerStartup() {
  listening_.Wait();
}

void ServerThread::WaitForCryptoHandshakeConfirmed() {
  confirmed_.Wait();
}

void ServerThread::Pause() {
  event_loop_mu_.Acquire();
}

void ServerThread::Resume() {
  event_loop_mu_.AssertAcquired();  // Checks the calling thread only!
  event_loop_mu_.Release();
}

void ServerThread::Quit() {
  quit_.Signal();
}

void ServerThread::MaybeNotifyOfHandshakeConfirmation() {
  if (confirmed_.IsSignaled()) {
    // Only notify once.
    return;
  }
  QuicDispatcher* dispatcher = QuicServerPeer::GetDispatcher(server());
  if (dispatcher->session_map().empty()) {
    // Wait for a session to be created.
    return;
  }
  QuicSession* session = dispatcher->session_map().begin()->second;
  if (session->IsCryptoHandshakeConfirmed()) {
    confirmed_.Signal();
  }
}

}  // namespace test
}  // namespace tools
}  // namespace net
