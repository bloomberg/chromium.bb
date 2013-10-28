// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/test_tools/server_thread.h"

namespace net {
namespace tools {
namespace test {

ServerThread::ServerThread(IPEndPoint address,
                           const QuicConfig& config,
                           const QuicVersionVector& supported_versions,
                           bool strike_register_no_startup_period)
    : SimpleThread("server_thread"),
      listening_(true, false),
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
  LOG(INFO) << "listening";
  server_.Listen(address_);

  port_lock_.Acquire();
  port_ = server_.port();
  port_lock_.Release();

  listening_.Signal();
  while (!quit_.IsSignaled()) {
    server_.WaitForEvents();
  }
  LOG(INFO) << "shutdown";
  server_.Shutdown();
}

int ServerThread::GetPort() {
  port_lock_.Acquire();
  int rc = port_;
  port_lock_.Release();
    return rc;
}

}  // namespace test
}  // namespace tools
}  // namespace net
