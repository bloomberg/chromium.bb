// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_quality_estimator_manager.h"

#include <algorithm>
#include <map>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/nqe/network_quality_estimator_params.h"
#include "services/network/public/cpp/network_switches.h"

namespace {

// Field trial for network quality estimator. Seeds RTT and downstream
// throughput observations with values that correspond to the connection type
// determined by the operating system.
const char kNetworkQualityEstimatorFieldTrialName[] = "NetworkQualityEstimator";

}  // namespace

namespace network {

NetworkQualityEstimatorManager::NetworkQualityEstimatorManager(
    net::NetLog* net_log) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

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

  network_quality_estimator_ = std::make_unique<net::NetworkQualityEstimator>(
      std::make_unique<net::NetworkQualityEstimatorParams>(
          network_quality_estimator_params),
      net_log);

#if defined(OS_CHROMEOS)
  // Get network id asynchronously to workaround https://crbug.com/821607 where
  // AddressTrackerLinux stucks with a recv() call and blocks IO thread.
  // TODO(https://crbug.com/821607): Remove after the bug is resolved.
  network_quality_estimator_->EnableGetNetworkIdAsynchronously();
#endif

  network_quality_estimator_->AddEffectiveConnectionTypeObserver(this);
  effective_connection_type_ =
      network_quality_estimator_->GetEffectiveConnectionType();
}

NetworkQualityEstimatorManager::~NetworkQualityEstimatorManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  network_quality_estimator_->RemoveEffectiveConnectionTypeObserver(this);
}

void NetworkQualityEstimatorManager::AddRequest(
    mojom::NetworkQualityEstimatorManagerRequest request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bindings_.AddBinding(this, std::move(request));
}

void NetworkQualityEstimatorManager::RequestNotifications(
    mojom::NetworkQualityEstimatorManagerClientPtr client_ptr) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  client_ptr->OnNetworkQualityChanged(effective_connection_type_);
  clients_.AddPtr(std::move(client_ptr));
}

void NetworkQualityEstimatorManager::OnEffectiveConnectionTypeChanged(
    net::EffectiveConnectionType effective_connection_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  effective_connection_type_ = effective_connection_type;
  clients_.ForAllPtrs([effective_connection_type](
                          mojom::NetworkQualityEstimatorManagerClient* client) {
    client->OnNetworkQualityChanged(effective_connection_type);
  });
}

net::NetworkQualityEstimator*
NetworkQualityEstimatorManager::GetNetworkQualityEstimator() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return network_quality_estimator_.get();
}

}  // namespace network
