// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/logging.h"

#if defined(CAST_STANDALONE_SENDER_HAVE_EXTERNAL_LIBS)
#include <getopt.h>

#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>
#include <vector>

#include "cast/common/certificate/cast_trust_store.h"
#include "cast/standalone_sender/constants.h"
#include "cast/standalone_sender/looping_file_cast_agent.h"
#include "cast/standalone_sender/receiver_chooser.h"
#include "cast/streaming/constants.h"
#include "platform/api/network_interface.h"
#include "platform/api/time.h"
#include "platform/base/error.h"
#include "platform/base/ip_address.h"
#include "platform/impl/platform_client_posix.h"
#include "platform/impl/task_runner.h"
#include "platform/impl/text_trace_logging_platform.h"
#include "util/chrono_helpers.h"
#include "util/stringprintf.h"

namespace openscreen {
namespace cast {
namespace {

void LogUsage(const char* argv0) {
  constexpr char kTemplate[] = R"(
usage: %s <options> network_interface media_file

or

usage: %s <options> addr[:port] media_file

   The first form runs this application in discovery+interactive mode. It will
   scan for Cast Receivers on the LAN reachable from the given network
   interface, and then the user will choose one interactively via a menu on the
   console.

   The second form runs this application in direct mode. It will not attempt to
   discover Cast Receivers, and instead connect directly to the Cast Receiver at
   addr:[port] (e.g., 192.168.1.22, 192.168.1.22:%d or [::1]:%d).

      -m, --max-bitrate=N
           Specifies the maximum bits per second for the media streams.

           Default if not set: %d
)"
#if defined(CAST_ALLOW_DEVELOPER_CERTIFICATE)
                               R"(
      -d, --developer-certificate=path-to-cert
           Specifies the path to a self-signed developer certificate that will
           be permitted for use as a root CA certificate for receivers that
           this sender instance will connect to. If omitted, only connections to
           receivers using an official Google-signed cast certificate chain will
           be permitted.
)"
#endif
                               R"(
      -a, --android-hack:
           Use the wrong RTP payload types, for compatibility with older Android
           TV receivers.

      -t, --tracing: Enable performance tracing logging.

      -v, --verbose: Enable verbose logging.

      -h, --help: Show this help message.
)";

  std::cerr << StringPrintf(kTemplate, argv0, argv0, kDefaultCastPort,
                            kDefaultCastPort, kDefaultMaxBitrate);
}

// Attempts to parse |string_form| into an IPEndpoint. The format is a
// standard-format IPv4 or IPv6 address followed by an optional colon and port.
// If the port is not provided, kDefaultCastPort is assumed.
//
// If the parse fails, a zero-port IPEndpoint is returned.
IPEndpoint ParseAsEndpoint(const char* string_form) {
  IPEndpoint result{};
  const ErrorOr<IPEndpoint> parsed_endpoint = IPEndpoint::Parse(string_form);
  if (parsed_endpoint.is_value()) {
    result = parsed_endpoint.value();
  } else {
    const ErrorOr<IPAddress> parsed_address = IPAddress::Parse(string_form);
    if (parsed_address.is_value()) {
      result = {parsed_address.value(), kDefaultCastPort};
    }
  }
  return result;
}

int StandaloneSenderMain(int argc, char* argv[]) {
  // A note about modifying command line arguments: consider uniformity
  // between all Open Screen executables. If it is a platform feature
  // being exposed, consider if it applies to the standalone receiver,
  // standalone sender, osp demo, and test_main argument options.
  const struct option kArgumentOptions[] = {
    {"max-bitrate", required_argument, nullptr, 'm'},
#if defined(CAST_ALLOW_DEVELOPER_CERTIFICATE)
    {"developer-certificate", required_argument, nullptr, 'd'},
#endif
    {"android-hack", no_argument, nullptr, 'a'},
    {"tracing", no_argument, nullptr, 't'},
    {"verbose", no_argument, nullptr, 'v'},
    {"help", no_argument, nullptr, 'h'},
    {nullptr, 0, nullptr, 0}
  };

  bool is_verbose = false;
  std::string developer_certificate_path;
  bool use_android_rtp_hack = false;
  int max_bitrate = kDefaultMaxBitrate;
  std::unique_ptr<TextTraceLoggingPlatform> trace_logger;
  int ch = -1;
  while ((ch = getopt_long(argc, argv, "m:d:atvh", kArgumentOptions,
                           nullptr)) != -1) {
    switch (ch) {
      case 'm':
        max_bitrate = atoi(optarg);
        if (max_bitrate < kMinRequiredBitrate) {
          OSP_LOG_ERROR << "Invalid --max-bitrate specified: " << optarg
                        << " is less than " << kMinRequiredBitrate;
          LogUsage(argv[0]);
          return 1;
        }
        break;
#if defined(CAST_ALLOW_DEVELOPER_CERTIFICATE)
      case 'd':
        developer_certificate_path = optarg;
        break;
#endif
      case 'a':
        use_android_rtp_hack = true;
        break;
      case 't':
        trace_logger = std::make_unique<TextTraceLoggingPlatform>();
        break;
      case 'v':
        is_verbose = true;
        break;
      case 'h':
        LogUsage(argv[0]);
        return 1;
    }
  }

  openscreen::SetLogLevel(is_verbose ? openscreen::LogLevel::kVerbose
                                     : openscreen::LogLevel::kInfo);
  // The second to last command line argument must be one of: 1) the network
  // interface name or 2) a specific IP address (port is optional). The last
  // argument must be the path to the file.
  if (optind != (argc - 2)) {
    LogUsage(argv[0]);
    return 1;
  }
  const char* const iface_or_endpoint = argv[optind++];
  const char* const path = argv[optind];

#if defined(CAST_ALLOW_DEVELOPER_CERTIFICATE)
  if (!developer_certificate_path.empty()) {
    CastTrustStore::CreateInstanceFromPemFile(developer_certificate_path);
  }
#endif

  auto* const task_runner = new TaskRunnerImpl(&Clock::now);
  PlatformClientPosix::Create(milliseconds(50),
                              std::unique_ptr<TaskRunnerImpl>(task_runner));

  IPEndpoint remote_endpoint = ParseAsEndpoint(iface_or_endpoint);
  if (!remote_endpoint.port) {
    for (const InterfaceInfo& interface : GetNetworkInterfaces()) {
      if (interface.name == iface_or_endpoint) {
        ReceiverChooser chooser(interface, task_runner,
                                [&](IPEndpoint endpoint) {
                                  remote_endpoint = endpoint;
                                  task_runner->RequestStopSoon();
                                });
        task_runner->RunUntilSignaled();
        break;
      }
    }

    if (!remote_endpoint.port) {
      OSP_LOG_ERROR << "No Cast Receiver chosen, or bad command-line argument. "
                       "Cannot continue.";
      LogUsage(argv[0]);
      return 2;
    }
  }

  // |cast_agent| must be constructed and destroyed from a Task run by the
  // TaskRunner.
  LoopingFileCastAgent* cast_agent = nullptr;
  task_runner->PostTask([&] {
    cast_agent = new LoopingFileCastAgent(
        task_runner, [&] { task_runner->RequestStopSoon(); });
    cast_agent->Connect({remote_endpoint, path, max_bitrate,
                         true /* should_include_video */,
                         use_android_rtp_hack});
  });

  // Run the event loop until SIGINT (e.g., CTRL-C at the console) or
  // SIGTERM are signaled.
  task_runner->RunUntilSignaled();

  // Spin the TaskRunner to destroy the |cast_agent| and execute any lingering
  // destruction/shutdown tasks.
  OSP_LOG_INFO << "Shutting down...";
  task_runner->PostTask([&] {
    delete cast_agent;
    task_runner->RequestStopSoon();
  });
  task_runner->RunUntilStopped();
  OSP_LOG_INFO << "Bye!";

  PlatformClientPosix::ShutDown();
  return 0;
}

}  // namespace
}  // namespace cast
}  // namespace openscreen
#endif

int main(int argc, char* argv[]) {
#if defined(CAST_STANDALONE_SENDER_HAVE_EXTERNAL_LIBS)
  return openscreen::cast::StandaloneSenderMain(argc, argv);
#else
  OSP_LOG_ERROR
      << "It compiled! However, you need to configure the build to point to "
         "external libraries in order to build a useful app.";
  return 1;
#endif
}
