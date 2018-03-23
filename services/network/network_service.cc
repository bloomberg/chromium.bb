// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_service.h"

#include <map>
#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/values.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/base/logging_network_change_observer.h"
#include "net/base/network_change_notifier.h"
#include "net/log/file_net_log_observer.h"
#include "net/log/net_log.h"
#include "net/log/net_log_util.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/nqe/network_quality_estimator_params.h"
#include "net/url_request/url_request_context_builder.h"
#include "services/network/network_context.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/url_request_context_builder_mojo.h"

namespace network {

namespace {

// Field trial for network quality estimator. Seeds RTT and downstream
// throughput observations with values that correspond to the connection type
// determined by the operating system.
const char kNetworkQualityEstimatorFieldTrialName[] = "NetworkQualityEstimator";

std::unique_ptr<net::NetworkChangeNotifier>
CreateNetworkChangeNotifierIfNeeded() {
  // There is a global singleton net::NetworkChangeNotifier if NetworkService
  // is running inside of the browser process.
  if (!net::NetworkChangeNotifier::HasNetworkChangeNotifier()) {
#if defined(OS_ANDROID)
    // On Android, NetworkChangeNotifier objects are always set up in process
    // before NetworkService is run.
    return nullptr;
#elif defined(OS_CHROMEOS) || defined(OS_IOS) || defined(OS_FUCHSIA)
    // ChromeOS has its own implementation of NetworkChangeNotifier that lives
    // outside of //net. iOS doesn't embed //content. Fuchsia doesn't have an
    // implementation yet.
    // TODO(xunjieli): Figure out what to do for these 3 platforms.
    NOTIMPLEMENTED();
    return nullptr;
#endif
    return base::WrapUnique(net::NetworkChangeNotifier::Create());
  }
  return nullptr;
}

}  // namespace

class NetworkService::MojoNetLog : public net::NetLog {
 public:
  MojoNetLog() {}

  // If specified by the command line, stream network events (NetLog) to a
  // file on disk. This will last for the duration of the process.
  void ProcessCommandLine(const base::CommandLine& command_line) {
    if (!command_line.HasSwitch(switches::kLogNetLog))
      return;

    base::FilePath log_path =
        command_line.GetSwitchValuePath(switches::kLogNetLog);

    // TODO(eroman): Should get capture mode from the command line.
    net::NetLogCaptureMode capture_mode =
        net::NetLogCaptureMode::IncludeCookiesAndCredentials();

    file_net_log_observer_ =
        net::FileNetLogObserver::CreateUnbounded(log_path, nullptr);
    file_net_log_observer_->StartObserving(this, capture_mode);
  }

  ~MojoNetLog() override {
    if (file_net_log_observer_)
      file_net_log_observer_->StopObserving(nullptr, base::OnceClosure());
  }

 private:
  std::unique_ptr<net::FileNetLogObserver> file_net_log_observer_;
  DISALLOW_COPY_AND_ASSIGN(MojoNetLog);
};

NetworkService::NetworkService(
    std::unique_ptr<service_manager::BinderRegistry> registry,
    mojom::NetworkServiceRequest request,
    net::NetLog* net_log)
    : registry_(std::move(registry)), binding_(this) {
  // |registry_| is nullptr when an in-process NetworkService is
  // created directly. The latter is done in concert with using
  // CreateNetworkContextWithBuilder to ease the transition to using the
  // network service.
  if (registry_) {
    DCHECK(!request.is_pending());
    registry_->AddInterface<mojom::NetworkService>(
        base::BindRepeating(&NetworkService::Bind, base::Unretained(this)));
  } else if (request.is_pending()) {
    Bind(std::move(request));
  }

  network_change_manager_ = std::make_unique<NetworkChangeManager>(
      CreateNetworkChangeNotifierIfNeeded());

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (net_log) {
    net_log_ = net_log;
  } else {
    owned_net_log_ = std::make_unique<MojoNetLog>();
    // Note: The command line switches are only checked when not using the
    // embedder's NetLog, as it may already be writing to the destination log
    // file.
    owned_net_log_->ProcessCommandLine(*command_line);
    net_log_ = owned_net_log_.get();
  }

  // Add an observer that will emit network change events to the ChromeNetLog.
  // Assuming NetworkChangeNotifier dispatches in FIFO order, we should be
  // logging the network change before other IO thread consumers respond to it.
  network_change_observer_.reset(
      new net::LoggingNetworkChangeObserver(net_log_));

  std::map<std::string, std::string> network_quality_estimator_params;
  base::GetFieldTrialParams(kNetworkQualityEstimatorFieldTrialName,
                            &network_quality_estimator_params);

  if (command_line->HasSwitch(switches::kForceEffectiveConnectionType)) {
    const std::string force_ect_value = command_line->GetSwitchValueASCII(
        switches::kForceEffectiveConnectionType);

    if (!force_ect_value.empty()) {
      // If the effective connection type is forced using command line switch,
      // it overrides the one set by field trial.
      network_quality_estimator_params[net::kForceEffectiveConnectionType] =
          force_ect_value;
    }
  }

  // Pass ownership.
  network_quality_estimator_ = std::make_unique<net::NetworkQualityEstimator>(
      std::make_unique<net::NetworkQualityEstimatorParams>(
          network_quality_estimator_params),
      net_log_);
}

NetworkService::~NetworkService() {
  // Call each Network and ask it to release its net::URLRequestContext, as they
  // may have references to shared objects owned by the NetworkService. The
  // NetworkContexts deregister themselves in Cleanup(), so have to be careful.
  while (!network_contexts_.empty())
    (*network_contexts_.begin())->Cleanup();
}

std::unique_ptr<NetworkService> NetworkService::Create(
    mojom::NetworkServiceRequest request,
    net::NetLog* net_log) {
  return std::make_unique<NetworkService>(nullptr, std::move(request), net_log);
}

std::unique_ptr<mojom::NetworkContext>
NetworkService::CreateNetworkContextWithBuilder(
    mojom::NetworkContextRequest request,
    mojom::NetworkContextParamsPtr params,
    std::unique_ptr<URLRequestContextBuilderMojo> builder,
    net::URLRequestContext** url_request_context) {
  std::unique_ptr<NetworkContext> network_context =
      std::make_unique<NetworkContext>(this, std::move(request),
                                       std::move(params), std::move(builder));
  *url_request_context = network_context->GetURLRequestContext();
  return network_context;
}

std::unique_ptr<NetworkService> NetworkService::CreateForTesting() {
  return base::WrapUnique(
      new NetworkService(std::make_unique<service_manager::BinderRegistry>()));
}

void NetworkService::RegisterNetworkContext(NetworkContext* network_context) {
  DCHECK_EQ(0u, network_contexts_.count(network_context));
  network_contexts_.insert(network_context);
  if (quic_disabled_)
    network_context->DisableQuic();
}

void NetworkService::DeregisterNetworkContext(NetworkContext* network_context) {
  DCHECK_EQ(1u, network_contexts_.count(network_context));
  network_contexts_.erase(network_context);
}

void NetworkService::SetClient(mojom::NetworkServiceClientPtr client) {
  client_ = std::move(client);
}

void NetworkService::CreateNetworkContext(
    mojom::NetworkContextRequest request,
    mojom::NetworkContextParamsPtr params) {
  // The NetworkContext will destroy itself on connection error, or when the
  // service is destroyed.
  new NetworkContext(this, std::move(request), std::move(params));
}

void NetworkService::DisableQuic() {
  quic_disabled_ = true;

  for (auto* network_context : network_contexts_) {
    network_context->DisableQuic();
  }
}

void NetworkService::SetRawHeadersAccess(uint32_t process_id, bool allow) {
  DCHECK(process_id);
  if (allow)
    processes_with_raw_headers_access_.insert(process_id);
  else
    processes_with_raw_headers_access_.erase(process_id);
}

bool NetworkService::HasRawHeadersAccess(uint32_t process_id) const {
  // Allow raw headers for browser-initiated requests.
  if (!process_id)
    return true;
  return processes_with_raw_headers_access_.find(process_id) !=
         processes_with_raw_headers_access_.end();
}

net::NetLog* NetworkService::net_log() const {
  return net_log_;
}

void NetworkService::GetNetworkChangeManager(
    mojom::NetworkChangeManagerRequest request) {
  network_change_manager_->AddRequest(std::move(request));
}

void NetworkService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_->BindInterface(interface_name, std::move(interface_pipe));
}

void NetworkService::Bind(mojom::NetworkServiceRequest request) {
  DCHECK(!binding_.is_bound());
  binding_.Bind(std::move(request));
}

}  // namespace network
