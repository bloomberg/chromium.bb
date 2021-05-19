// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/standalone_receiver/cast_service.h"

#include <utility>

#include "discovery/common/config.h"
#include "platform/api/tls_connection_factory.h"
#include "platform/base/interface_info.h"
#include "platform/base/tls_listen_options.h"
#include "util/osp_logging.h"
#include "util/stringprintf.h"

namespace openscreen {
namespace cast {

namespace {

constexpr uint16_t kDefaultCastServicePort = 8010;

constexpr int kDefaultMaxBacklogSize = 64;
const TlsListenOptions kDefaultListenOptions{kDefaultMaxBacklogSize};

IPEndpoint DetermineEndpoint(const InterfaceInfo& interface) {
  const IPAddress address = interface.GetIpAddressV4()
                                ? interface.GetIpAddressV4()
                                : interface.GetIpAddressV6();
  OSP_CHECK(address);
  return IPEndpoint{address, kDefaultCastServicePort};
}

discovery::Config MakeDiscoveryConfig(const InterfaceInfo& interface) {
  discovery::Config config;

  discovery::Config::NetworkInfo::AddressFamilies supported_address_families =
      discovery::Config::NetworkInfo::kNoAddressFamily;
  if (interface.GetIpAddressV4()) {
    supported_address_families |= discovery::Config::NetworkInfo::kUseIpV4;
  } else if (interface.GetIpAddressV6()) {
    supported_address_families |= discovery::Config::NetworkInfo::kUseIpV6;
  }
  config.network_info.push_back({interface, supported_address_families});

  return config;
}

}  // namespace

CastService::CastService(TaskRunner* task_runner,
                         const InterfaceInfo& interface,
                         GeneratedCredentials credentials,
                         const std::string& friendly_name,
                         const std::string& model_name,
                         bool enable_discovery)
    : local_endpoint_(DetermineEndpoint(interface)),
      credentials_(std::move(credentials)),
      agent_(task_runner, credentials_.provider.get()),
      mirroring_application_(task_runner, local_endpoint_.address, &agent_),
      socket_factory_(&agent_, agent_.cast_socket_client()),
      connection_factory_(
          TlsConnectionFactory::CreateFactory(&socket_factory_, task_runner)),
      discovery_service_(enable_discovery ? discovery::CreateDnsSdService(
                                                task_runner,
                                                this,
                                                MakeDiscoveryConfig(interface))
                                          : LazyDeletedDiscoveryService()),
      discovery_publisher_(
          discovery_service_
              ? MakeSerialDelete<discovery::DnsSdServicePublisher<ServiceInfo>>(
                    task_runner,
                    discovery_service_.get(),
                    kCastV2ServiceId,
                    ServiceInfoToDnsSdInstance)
              : LazyDeletedDiscoveryPublisher()) {
  connection_factory_->SetListenCredentials(credentials_.tls_credentials);
  connection_factory_->Listen(local_endpoint_, kDefaultListenOptions);

  if (discovery_publisher_) {
    ServiceInfo info;
    info.port = local_endpoint_.port;
    info.unique_id = HexEncode(interface.hardware_address);
    info.friendly_name = friendly_name;
    info.model_name = model_name;
    info.capabilities = kHasVideoOutput | kHasAudioOutput;
    Error error = discovery_publisher_->Register(info);
    if (!error.ok()) {
      OnFatalError(std::move(error));
    }
  }
}

CastService::~CastService() {
  if (discovery_publisher_) {
    discovery_publisher_->DeregisterAll();
  }
}

void CastService::OnFatalError(Error error) {
  OSP_LOG_FATAL << "Encountered fatal discovery error: " << error;
}

void CastService::OnRecoverableError(Error error) {
  OSP_LOG_ERROR << "Encountered recoverable discovery error: " << error;
}

}  // namespace cast
}  // namespace openscreen
