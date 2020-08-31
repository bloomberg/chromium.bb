// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <getopt.h>

#include <array>
#include <chrono>  // NOLINT
#include <iostream>

#include "cast/common/public/service_info.h"
#include "cast/standalone_receiver/cast_agent.h"
#include "cast/streaming/ssrc.h"
#include "discovery/common/config.h"
#include "discovery/common/reporting_client.h"
#include "discovery/public/dns_sd_service_factory.h"
#include "discovery/public/dns_sd_service_publisher.h"
#include "platform/api/time.h"
#include "platform/api/udp_socket.h"
#include "platform/base/error.h"
#include "platform/base/ip_address.h"
#include "platform/impl/logging.h"
#include "platform/impl/network_interface.h"
#include "platform/impl/platform_client_posix.h"
#include "platform/impl/task_runner.h"
#include "platform/impl/text_trace_logging_platform.h"
#include "util/stringprintf.h"
#include "util/trace_logging.h"

namespace openscreen {
namespace cast {
namespace {

class DiscoveryReportingClient : public discovery::ReportingClient {
  void OnFatalError(Error error) override {
    OSP_LOG_FATAL << "Encountered fatal discovery error: " << error;
  }

  void OnRecoverableError(Error error) override {
    OSP_LOG_ERROR << "Encountered recoverable discovery error: " << error;
  }
};

struct DiscoveryState {
  SerialDeletePtr<discovery::DnsSdService> service;
  std::unique_ptr<DiscoveryReportingClient> reporting_client;
  std::unique_ptr<discovery::DnsSdServicePublisher<ServiceInfo>> publisher;
};

ErrorOr<std::unique_ptr<DiscoveryState>> StartDiscovery(
    TaskRunner* task_runner,
    const InterfaceInfo& interface) {
  discovery::Config config;

  discovery::Config::NetworkInfo::AddressFamilies supported_address_families =
      discovery::Config::NetworkInfo::kNoAddressFamily;
  if (interface.GetIpAddressV4()) {
    supported_address_families |= discovery::Config::NetworkInfo::kUseIpV4;
  }
  if (interface.GetIpAddressV6()) {
    supported_address_families |= discovery::Config::NetworkInfo::kUseIpV6;
  }
  OSP_CHECK(supported_address_families !=
            discovery::Config::NetworkInfo::kNoAddressFamily)
      << "No address families supported by the selected interface";
  config.network_info.push_back({interface, supported_address_families});

  auto state = std::make_unique<DiscoveryState>();
  state->reporting_client = std::make_unique<DiscoveryReportingClient>();
  state->service = discovery::CreateDnsSdService(
      task_runner, state->reporting_client.get(), config);

  ServiceInfo info;
  info.port = kDefaultCastPort;

  OSP_CHECK(std::any_of(interface.hardware_address.begin(),
                        interface.hardware_address.end(),
                        [](int e) { return e > 0; }));
  info.unique_id = HexEncode(interface.hardware_address);

  // TODO(jophba): add command line arguments to set these fields.
  info.model_name = "cast_standalone_receiver";
  info.friendly_name = "Cast Standalone Receiver";

  state->publisher =
      std::make_unique<discovery::DnsSdServicePublisher<ServiceInfo>>(
          state->service.get(), kCastV2ServiceId, ServiceInfoToDnsSdInstance);

  auto error = state->publisher->Register(info);
  if (!error.ok()) {
    return error;
  }
  return state;
}

void StartCastAgent(TaskRunnerImpl* task_runner, InterfaceInfo interface) {
  CastAgent agent(task_runner, interface);
  const auto error = agent.Start();
  if (!error.ok()) {
    OSP_LOG_ERROR << "Error occurred while starting agent: " << error;
    return;
  }

  // Run the event loop until an exit is requested (e.g., the video player GUI
  // window is closed, a SIGINT or SIGTERM is received, or whatever other
  // appropriate user indication that shutdown is requested).
  task_runner->RunUntilSignaled();
}

void LogUsage(const char* argv0) {
  std::cerr << R"(
usage: )" << argv0
            << R"( <options> <interface>

options:
    interface
        Specifies the network interface to bind to. The interface is
        looked up from the system interface registry. This argument is
        mandatory, as it must be known for publishing discovery.

    -t, --tracing: Enable performance tracing logging.

    -v, --verbose: Enable verbose logging.

    -h, --help: Show this help message.
  )";
}

InterfaceInfo GetInterfaceInfoFromName(const char* name) {
  OSP_CHECK(name != nullptr) << "Missing mandatory argument: interface.";
  InterfaceInfo interface_info;
  std::vector<InterfaceInfo> network_interfaces = GetNetworkInterfaces();
  for (auto& interface : network_interfaces) {
    if (interface.name == name) {
      interface_info = std::move(interface);
      break;
    }
  }
  OSP_CHECK(!interface_info.name.empty()) << "Invalid interface specified.";
  return interface_info;
}

int RunStandaloneReceiver(int argc, char* argv[]) {
  // A note about modifying command line arguments: consider uniformity
  // between all Open Screen executables. If it is a platform feature
  // being exposed, consider if it applies to the standalone receiver,
  // standalone sender, osp demo, and test_main argument options.
  const struct option kArgumentOptions[] = {
      {"tracing", no_argument, nullptr, 't'},
      {"verbose", no_argument, nullptr, 'v'},
      {"help", no_argument, nullptr, 'h'},
      {nullptr, 0, nullptr, 0}};

  bool is_verbose = false;
  std::unique_ptr<openscreen::TextTraceLoggingPlatform> trace_logger;
  int ch = -1;
  while ((ch = getopt_long(argc, argv, "tvh", kArgumentOptions, nullptr)) !=
         -1) {
    switch (ch) {
      case 't':
        trace_logger = std::make_unique<openscreen::TextTraceLoggingPlatform>();
        break;
      case 'v':
        is_verbose = true;
        break;
      case 'h':
        LogUsage(argv[0]);
        return 1;
    }
  }
  InterfaceInfo interface_info = GetInterfaceInfoFromName(argv[optind]);
  SetLogLevel(is_verbose ? openscreen::LogLevel::kVerbose
                         : openscreen::LogLevel::kInfo);

  auto* const task_runner = new TaskRunnerImpl(&Clock::now);
  PlatformClientPosix::Create(Clock::duration{50}, Clock::duration{50},
                              std::unique_ptr<TaskRunnerImpl>(task_runner));

  auto discovery_state = StartDiscovery(task_runner, interface_info);
  OSP_CHECK(discovery_state.is_value()) << "Failed to start discovery.";

  // Runs until the process is interrupted.  Safe to pass |task_runner| as it
  // will not be destroyed by ShutDown() until this exits.
  StartCastAgent(task_runner, interface_info);

  // The task runner must be deleted after all serial delete pointers, such
  // as the one stored in the discovery state.
  discovery_state.value().reset();
  PlatformClientPosix::ShutDown();
  return 0;
}

}  // namespace
}  // namespace cast
}  // namespace openscreen

int main(int argc, char* argv[]) {
  return openscreen::cast::RunStandaloneReceiver(argc, argv);
}
