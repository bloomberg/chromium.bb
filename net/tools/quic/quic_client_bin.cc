// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A binary wrapper for QuicClient.  Connects to --hostname or --address on
// --port and requests URLs specified on the command line.
//
// For example:
//  quic_client --port=6122 /index.html /favicon.ico

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "net/base/ip_endpoint.h"
#include "net/quic/quic_protocol.h"
#include "net/tools/quic/quic_client.h"

int32 FLAGS_port = 6121;
std::string FLAGS_address = "127.0.0.1";
std::string FLAGS_hostname = "localhost";

int main(int argc, char *argv[]) {
  CommandLine::Init(argc, argv);
  CommandLine* line = CommandLine::ForCurrentProcess();
  if (line->HasSwitch("port")) {
    int port;
    if (base::StringToInt(line->GetSwitchValueASCII("port"), &port)) {
      FLAGS_port = port;
    }
  }
  if (line->HasSwitch("address")) {
    FLAGS_address = line->GetSwitchValueASCII("address");
  }
  if (line->HasSwitch("hostname")) {
    FLAGS_hostname = line->GetSwitchValueASCII("hostname");
  }
  LOG(INFO) << "server port: " << FLAGS_port
            << " address: " << FLAGS_address
            << " hostname: " << FLAGS_hostname;

  base::AtExitManager exit_manager;

  net::IPAddressNumber addr;
  CHECK(net::ParseIPLiteralToNumber(FLAGS_address, &addr));
  // TODO(rjshade): Set version on command line.
  net::tools::QuicClient client(
      net::IPEndPoint(addr, FLAGS_port), FLAGS_hostname, net::QuicVersionMax());

  client.Initialize();

  if (!client.Connect()) return 1;

  client.SendRequestsAndWaitForResponse(line->GetArgs());
  return 0;
}
