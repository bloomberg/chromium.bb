// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/logging.h"

#if defined(CAST_STANDALONE_SENDER_HAVE_EXTERNAL_LIBS)
#include <getopt.h>

#include <chrono>
#include <cinttypes>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>

#include "cast/common/certificate/cast_trust_store.h"
#include "cast/standalone_sender/constants.h"
#include "cast/standalone_sender/looping_file_cast_agent.h"
#include "cast/streaming/constants.h"
#include "cast/streaming/environment.h"
#include "cast/streaming/sender.h"
#include "cast/streaming/sender_packet_router.h"
#include "cast/streaming/session_config.h"
#include "cast/streaming/ssrc.h"
#include "platform/api/time.h"
#include "platform/base/error.h"
#include "platform/base/ip_address.h"
#include "platform/impl/platform_client_posix.h"
#include "platform/impl/task_runner.h"
#include "platform/impl/text_trace_logging_platform.h"
#include "util/alarm.h"
#include "util/chrono_helpers.h"
#include "util/stringprintf.h"

namespace openscreen {
namespace cast {
namespace {

IPEndpoint GetDefaultEndpoint() {
  return IPEndpoint{IPAddress::kV4LoopbackAddress(), kDefaultCastPort};
}

void LogUsage(const char* argv0) {
  constexpr char kTemplate[] = R"(
usage: %s <options> <media_file>

      -r, --remote=addr[:port]
           Specify the destination (e.g., 192.168.1.22:9999 or [::1]:12345).

           Default if not set: %s

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

  std::cerr << StringPrintf(kTemplate, argv0,
                            GetDefaultEndpoint().ToString().c_str(),
                            kDefaultMaxBitrate);
}

int StandaloneSenderMain(int argc, char* argv[]) {
  // A note about modifying command line arguments: consider uniformity
  // between all Open Screen executables. If it is a platform feature
  // being exposed, consider if it applies to the standalone receiver,
  // standalone sender, osp demo, and test_main argument options.
  const struct option kArgumentOptions[] = {
    {"remote", required_argument, nullptr, 'r'},
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
  IPEndpoint remote_endpoint = GetDefaultEndpoint();
  std::string developer_certificate_path;
  [[maybe_unused]] bool use_android_rtp_hack = false;
  [[maybe_unused]] int max_bitrate = kDefaultMaxBitrate;
  std::unique_ptr<TextTraceLoggingPlatform> trace_logger;
  int ch = -1;
  while ((ch = getopt_long(argc, argv, "r:m:d:atvh", kArgumentOptions,
                           nullptr)) != -1) {
    switch (ch) {
      case 'r': {
        const ErrorOr<IPEndpoint> parsed_endpoint = IPEndpoint::Parse(optarg);
        if (parsed_endpoint.is_value()) {
          remote_endpoint = parsed_endpoint.value();
        } else {
          const ErrorOr<IPAddress> parsed_address = IPAddress::Parse(optarg);
          if (parsed_address.is_value()) {
            remote_endpoint.address = parsed_address.value();
          } else {
            OSP_LOG_ERROR << "Invalid --remote specified: " << optarg;
            LogUsage(argv[0]);
            return 1;
          }
        }
        break;
      }
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
  // The last command line argument must be the path to the file.
  const char* path = nullptr;
  if (optind == (argc - 1)) {
    path = argv[optind];
  }

  if (!path || !remote_endpoint.port) {
    LogUsage(argv[0]);
    return 1;
  }

#if defined(CAST_ALLOW_DEVELOPER_CERTIFICATE)
  if (!developer_certificate_path.empty()) {
    CastTrustStore::CreateInstanceFromPemFile(developer_certificate_path);
  }
#endif

  auto* const task_runner = new TaskRunnerImpl(&Clock::now);
  PlatformClientPosix::Create(Clock::duration{50}, Clock::duration{50},
                              std::unique_ptr<TaskRunnerImpl>(task_runner));

  std::unique_ptr<LoopingFileCastAgent> cast_agent;
  task_runner->PostTask([&] {
    cast_agent = std::make_unique<LoopingFileCastAgent>(task_runner);
    cast_agent->Connect({remote_endpoint, path, max_bitrate,
                         true /* should_include_video */,
                         use_android_rtp_hack});
  });

  // Run the event loop until SIGINT (e.g., CTRL-C at the console) or
  // SIGTERM are signaled.
  task_runner->RunUntilSignaled();

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
