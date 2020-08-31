// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_diagnostics/lan_connectivity_routine.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/chromeos/net/network_diagnostics/network_diagnostics.mojom.h"
#include "chromeos/services/network_config/in_process_instance.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"

namespace chromeos {
namespace network_diagnostics {

namespace {

using chromeos::network_config::mojom::CrosNetworkConfig;
using chromeos::network_config::mojom::FilterType;
using chromeos::network_config::mojom::NetworkFilter;
using chromeos::network_config::mojom::NetworkStatePropertiesPtr;
using chromeos::network_config::mojom::NetworkType;

void GetNetworkConfigService(
    mojo::PendingReceiver<CrosNetworkConfig> receiver) {
  chromeos::network_config::BindToInProcessInstance(std::move(receiver));
}

}  // namespace

LanConnectivityRoutine::LanConnectivityRoutine() {
  set_verdict(mojom::RoutineVerdict::kNotRun);
  GetNetworkConfigService(
      remote_cros_network_config_.BindNewPipeAndPassReceiver());
}

LanConnectivityRoutine::~LanConnectivityRoutine() = default;

bool LanConnectivityRoutine::CanRun() {
  DCHECK(remote_cros_network_config_);
  return true;
}

void LanConnectivityRoutine::RunTest(LanConnectivityRoutineCallback callback) {
  if (!CanRun()) {
    std::move(callback).Run(verdict());
    return;
  }
  routine_completed_callback_ = std::move(callback);
  FetchActiveNetworks();
}

void LanConnectivityRoutine::AnalyzeResultsAndExecuteCallback() {
  if (lan_connected_) {
    set_verdict(mojom::RoutineVerdict::kNoProblem);
  } else {
    set_verdict(mojom::RoutineVerdict::kProblem);
  }
  std::move(routine_completed_callback_).Run(verdict());
}

void LanConnectivityRoutine::FetchActiveNetworks() {
  DCHECK(remote_cros_network_config_);
  // The usage of `base::Unretained(this)` here is safe because
  // |remote_cros_network_config_| is a mojo::Remote owned by |this|.
  remote_cros_network_config_->GetNetworkStateList(
      NetworkFilter::New(FilterType::kActive, NetworkType::kAll,
                         network_config::mojom::kNoLimit),
      base::BindOnce(&LanConnectivityRoutine::OnNetworkStateListReceived,
                     base::Unretained(this)));
}

// Process the network interface information.
void LanConnectivityRoutine::OnNetworkStateListReceived(
    std::vector<NetworkStatePropertiesPtr> networks) {
  for (const NetworkStatePropertiesPtr& network : networks) {
    if (network_config::StateIsConnected(network->connection_state)) {
      lan_connected_ = true;
      break;
    }
  }
  AnalyzeResultsAndExecuteCallback();
}

}  // namespace network_diagnostics
}  // namespace chromeos
