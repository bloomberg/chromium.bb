// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A binary wrapper for QuicServer.  It listens forever on --port
// (default 6121) until it's killed or ctrl-cd to death.

#include "base/at_exit.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "net/base/ip_endpoint.h"
#include "net/tools/quic/quic_server.h"

// The port the quic server will listen on.
int32 FLAGS_port = 6121;

int main(int argc, char *argv[]) {
  CommandLine::Init(argc, argv);

  base::AtExitManager exit_manager;

  net::IPAddressNumber ip;
  CHECK(net::ParseIPLiteralToNumber("::", &ip));

  net::QuicServer server;

  if (!server.Listen(net::IPEndPoint(ip, FLAGS_port))) {
    return 1;
  }

  while (1) {
    server.WaitForEvents();
  }

  return 0;
}
