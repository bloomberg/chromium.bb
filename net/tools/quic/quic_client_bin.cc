// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A binary wrapper for QuicClient.
// Connects to a host using QUIC, and sends requests to the provided URLS.
//
// Example usage:
//  quic_client --address=127.0.0.1 --port=6122 --hostname=www.google.com
//      http://www.google.com/index.html http://www.google.com/favicon.ico

#include <iostream>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "net/base/ip_endpoint.h"
#include "net/base/privacy_mode.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_server_id.h"
#include "net/tools/epoll_server/epoll_server.h"
#include "net/tools/quic/quic_client.h"

std::string FLAGS_address = "127.0.0.1";
// The IP or hostname the quic client will connect to.
std::string FLAGS_hostname = "localhost";
// The port the quic client will connect to.
int32 FLAGS_port = 6121;
// Check the certificates using proof verifier.
bool FLAGS_secure = false;
// QUIC version to speak, e.g. 21. Default value of 0 means 'use the latest
// version'.
int32 FLAGS_quic_version = 0;
// Size of flow control receive window to advertize to the peer.
int32 FLAGS_flow_control_window_bytes = 10 * 1024 * 1024;  // 10 Mb

int main(int argc, char *argv[]) {
  base::CommandLine::Init(argc, argv);
  base::CommandLine* line = base::CommandLine::ForCurrentProcess();
  const base::CommandLine::StringVector& urls = line->GetArgs();

  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  CHECK(logging::InitLogging(settings));

  if (line->HasSwitch("h") || line->HasSwitch("help") || urls.empty()) {
    const char* help_str =
        "Usage: quic_client [options] <url> ...\n"
        "\n"
        "At least one <url> with scheme must be provided "
        "(e.g. http://www.google.com/)\n\n"
        "Options:\n"
        "-h, --help                  show this help message and exit\n"
        "--port=<port>               specify the port to connect to\n"
        "--address=<address>         specify the IP address to connect to\n"
        "--host=<host>               specify the SNI hostname to use\n"
        "--secure                    check certificates\n"
        "--quic-version=<quic version> specify QUIC version to speak\n"
        "--flow-control-window-bytes=<bytes> specify size of flow control "
        "receive window to advertize to the peer\n";
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
  if (line->HasSwitch("secure")) {
    FLAGS_secure = true;
  }
  if (line->HasSwitch("quic-version")) {
    int quic_version;
    if (base::StringToInt(line->GetSwitchValueASCII("quic-version"),
                          &quic_version)) {
      FLAGS_quic_version = quic_version;
    }
  }
  if (line->HasSwitch("flow-control-window-bytes")) {
    int flow_control_window_bytes;
    if (base::StringToInt(
            line->GetSwitchValueASCII("flow-control-window-bytes"),
            &flow_control_window_bytes)) {
      FLAGS_flow_control_window_bytes = flow_control_window_bytes;
    }
  }
  VLOG(1) << "server port: " << FLAGS_port
          << " address: " << FLAGS_address
          << " hostname: " << FLAGS_hostname
          << " secure: " << FLAGS_secure
          << " quic-version: " << FLAGS_quic_version;

  base::AtExitManager exit_manager;

  // Determine IP address to connect to from supplied hostname.
  net::IPAddressNumber addr;
  CHECK(net::ParseIPLiteralToNumber(FLAGS_address, &addr));

  // Populate version vector with all versions if none specified.
  net::QuicVersionVector versions;
  if (FLAGS_quic_version == 0) {
    versions = net::QuicSupportedVersions();
  } else {
    versions.push_back(static_cast<net::QuicVersion>(FLAGS_quic_version));
  }

  // Build the client, and try to connect.
  VLOG(1) << "Conecting to " << FLAGS_hostname << ":" << FLAGS_port
          << " with supported versions "
          << QuicVersionVectorToString(versions);
  net::EpollServer epoll_server;
  net::QuicConfig config;
  config.SetDefaults();

  // The default flow control window of 16 Kb is too small for practical
  // purposes. Set it to the specified value, which has a large default.
  config.SetInitialFlowControlWindowToSend(
      FLAGS_flow_control_window_bytes);
  config.SetInitialStreamFlowControlWindowToSend(
      FLAGS_flow_control_window_bytes);
  config.SetInitialSessionFlowControlWindowToSend(
      FLAGS_flow_control_window_bytes);

  net::tools::QuicClient client(
      net::IPEndPoint(addr, FLAGS_port),
      net::QuicServerId(FLAGS_hostname, FLAGS_port, FLAGS_secure,
                        net::PRIVACY_MODE_DISABLED),
      versions, true, config, &epoll_server);

  client.Initialize();

  if (!client.Connect()) {
    LOG(ERROR) << "Client failed to connect to host: "
               << FLAGS_hostname << ":" << FLAGS_port;
    return 1;
  }

  // Send a GET request for each supplied url.
  client.SendRequestsAndWaitForResponse(urls);
  return 0;
}
