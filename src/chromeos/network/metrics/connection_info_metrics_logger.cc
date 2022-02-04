// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/metrics/connection_info_metrics_logger.h"

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "chromeos/network/metrics/network_metrics_helper.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"

namespace chromeos {

ConnectionInfoMetricsLogger::ConnectionInfo::ConnectionInfo(
    const NetworkState* network,
    bool was_disconnect_requested)
    : guid(network->guid()),
      shill_error(network->GetError()),
      was_disconnect_requested(was_disconnect_requested) {
  if (network->IsConnectedState())
    status = Status::kConnected;
  else if (network->IsConnectingState())
    status = Status::kConnecting;
  else
    status = Status::kDisconnected;
}

ConnectionInfoMetricsLogger::ConnectionInfo::~ConnectionInfo() = default;

bool ConnectionInfoMetricsLogger::ConnectionInfo::operator==(
    const ConnectionInfoMetricsLogger::ConnectionInfo& other) const {
  return status == other.status &&
         was_disconnect_requested == other.was_disconnect_requested &&
         guid == other.guid && shill_error == other.shill_error;
}

ConnectionInfoMetricsLogger::ConnectionInfoMetricsLogger() = default;

ConnectionInfoMetricsLogger::~ConnectionInfoMetricsLogger() {
  if (network_state_handler_)
    network_state_handler_->RemoveObserver(this, FROM_HERE);
  if (network_connection_handler_)
    network_connection_handler_->RemoveObserver(this);
}

void ConnectionInfoMetricsLogger::Init(
    NetworkStateHandler* network_state_handler,
    NetworkConnectionHandler* network_connection_handler) {
  if (network_connection_handler) {
    network_connection_handler_ = network_connection_handler;
    network_connection_handler_->AddObserver(this);
  }

  if (network_state_handler) {
    network_state_handler_ = network_state_handler;
    network_state_handler_->AddObserver(this, FROM_HERE);
    NetworkListChanged();
  }
}

void ConnectionInfoMetricsLogger::NetworkListChanged() {
  NetworkStateHandler::NetworkStateList network_list;
  network_state_handler_->GetVisibleNetworkList(&network_list);

  base::flat_set<std::string> visible_guids;
  for (const auto* network : network_list) {
    UpdateConnectionInfo(network);
    visible_guids.insert(network->guid());
  }

  base::flat_map<std::string, ConnectionInfo> old_guid_to_connection_info;
  old_guid_to_connection_info.swap(guid_to_connection_info_);

  // Only store visible network ConnectionInfo in |guid_to_connection_info_|.
  for (const auto& connection_info : old_guid_to_connection_info) {
    const std::string& guid = connection_info.first;
    if (!base::Contains(visible_guids, guid))
      continue;
    guid_to_connection_info_.insert_or_assign(
        guid, old_guid_to_connection_info.find(guid)->second);
  }
}

void ConnectionInfoMetricsLogger::NetworkConnectionStateChanged(
    const NetworkState* network) {
  UpdateConnectionInfo(network);
}

void ConnectionInfoMetricsLogger::DisconnectRequested(
    const std::string& service_path) {
  if (!network_state_handler_)
    return;

  const NetworkState* network =
      network_state_handler_->GetNetworkState(service_path);

  if (!network)
    return;

  UpdateConnectionInfo(network, /*disconnect_requested=*/true);
}

void ConnectionInfoMetricsLogger::ConnectSucceeded(
    const std::string& service_path) {
  const NetworkState* network =
      network_state_handler_->GetNetworkState(service_path);

  if (!network)
    return;

  NetworkMetricsHelper::LogUserInitiatedConnectionResult(network->guid());
}

void ConnectionInfoMetricsLogger::ConnectFailed(const std::string& service_path,
                                                const std::string& error_name) {
  const NetworkState* network =
      network_state_handler_->GetNetworkState(service_path);

  if (!network)
    return;

  NetworkMetricsHelper::LogUserInitiatedConnectionResult(network->guid(),
                                                         error_name);
}

void ConnectionInfoMetricsLogger::UpdateConnectionInfo(
    const NetworkState* network,
    bool disconnect_requested) {
  const absl::optional<ConnectionInfo> prev_info =
      GetCachedInfo(network->guid());
  ConnectionInfo curr_info =
      ConnectionInfo(network,
                     /*was_disconnect_requested=*/disconnect_requested);

  // No updates if the ConnectionInfo did not change.
  if (prev_info == curr_info)
    return;

  // If the connection status has changed, attempt to log automatic connection
  // and disconnection metrics. Otherwise, if a disconnect has been requested,
  // maintain the request until the status changes.
  if (!prev_info || prev_info->status != curr_info.status) {
    AttemptLogAllConnectionResult(prev_info, curr_info);
    AttemptLogConnectionStateResult(prev_info, curr_info);
  } else if (prev_info && prev_info->was_disconnect_requested) {
    curr_info.was_disconnect_requested = true;
  }
  guid_to_connection_info_.insert_or_assign(network->guid(), curr_info);
}

void ConnectionInfoMetricsLogger::AttemptLogConnectionStateResult(
    const absl::optional<ConnectionInfo>& prev_info,
    const ConnectionInfo& curr_info) const {
  if (curr_info.status == ConnectionInfo::Status::kConnected) {
    NetworkMetricsHelper::LogConnectionStateResult(
        curr_info.guid, NetworkMetricsHelper::ConnectionState::kConnected);
    return;
  }

  // If the network becomes disconnected from a connected state without a
  // user initiated disconnect request.
  if (prev_info && prev_info->status == ConnectionInfo::Status::kConnected &&
      curr_info.status == ConnectionInfo::Status::kDisconnected &&
      (!prev_info->was_disconnect_requested)) {
    NetworkMetricsHelper::LogConnectionStateResult(
        curr_info.guid,
        NetworkMetricsHelper::ConnectionState::kDisconnectedWithoutUserAction);
    return;
  }
}

void ConnectionInfoMetricsLogger::AttemptLogAllConnectionResult(
    const absl::optional<ConnectionInfo>& prev_info,
    const ConnectionInfo& curr_info) const {
  DCHECK(!prev_info || prev_info && prev_info->guid == curr_info.guid);

  if (curr_info.status == ConnectionInfo::Status::kConnected)
    NetworkMetricsHelper::LogAllConnectionResult(curr_info.guid);

  if (prev_info && !prev_info->was_disconnect_requested &&
      prev_info->status == ConnectionInfo::Status::kConnecting &&
      curr_info.status == ConnectionInfo::Status::kDisconnected) {
    NetworkMetricsHelper::LogAllConnectionResult(curr_info.guid,
                                                 curr_info.shill_error);
  }
}

absl::optional<ConnectionInfoMetricsLogger::ConnectionInfo>
ConnectionInfoMetricsLogger::GetCachedInfo(const std::string& guid) const {
  const auto prev_info_it = guid_to_connection_info_.find(guid);
  if (prev_info_it == guid_to_connection_info_.end())
    return absl::nullopt;
  return prev_info_it->second;
}

}  // namespace chromeos