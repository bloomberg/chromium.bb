// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A binary wrapper for QuicClient.  Connects to --hostname via --address
// on --port and requests URLs specified on the command line.
//
// For example:
//  quic_client --address=127.0.0.1 --port=6122 --hostname=www.google.com
//      http://www.google.com/index.html http://www.google.com/favicon.ico

#include <iostream>

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

  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  CHECK(logging::InitLogging(settings));

  if (line->HasSwitch("h") || line->HasSwitch("help")) {
    const char* help_str =
        "Usage: quic_client [options]\n"
        "\n"
        "Options:\n"
        "-h, --help                  show this help message and exit\n"
        "--port=<port>               specify the port to connect to\n"
        "--address=<address>         specify the IP address to connect to\n"
        "--host=<host>               specify the SNI hostname to use\n";
    std::cout << help_str;
    exit(0);
  }
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
  VLOG(1) << "server port: " << FLAGS_port
          << " address: " << FLAGS_address
          << " hostname: " << FLAGS_hostname;

  base::AtExitManager exit_manager;

  net::IPAddressNumber addr;
  CHECK(net::ParseIPLiteralToNumber(FLAGS_address, &addr));
  // TODO(rjshade): Set version on command line.
  net::tools::QuicClient client(
      net::IPEndPoint(addr, FLAGS_port), FLAGS_hostname,
      net::QuicSupportedVersions(), true);

  client.Initialize();

  if (!client.Connect()) return 1;

  client.SendRequestsAndWaitForResponse(line->GetArgs());
  return 0;
}
