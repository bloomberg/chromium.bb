// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A binary wrapper for QuicServer.  It listens forever on --port
// (default 6121) until it's killed or ctrl-cd to death.

#include <vector>

#include "quic/core/quic_versions.h"
#include "quic/platform/api/quic_flags.h"
#include "quic/platform/api/quic_system_event_loop.h"
#include "quic/tools/quic_epoll_server_factory.h"
#include "quic/tools/quic_toy_server.h"

int main(int argc, char* argv[]) {
  QuicSystemEventLoop event_loop("quic_server");
  const char* usage = "Usage: quic_server [options]";
  std::vector<std::string> non_option_args =
      quic::QuicParseCommandLineFlags(usage, argc, argv);
  if (!non_option_args.empty()) {
    quic::QuicPrintCommandLineFlagHelp(usage);
    exit(0);
  }

  quic::QuicToyServer::MemoryCacheBackendFactory backend_factory;
  quic::QuicEpollServerFactory server_factory;
  quic::QuicToyServer server(&backend_factory, &server_factory);
  return server.Start();
}
