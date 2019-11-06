// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/network/network_status_listener_impl.h"

#include "base/bind.h"

namespace download {

NetworkStatusListenerImpl::NetworkStatusListenerImpl(
    network::NetworkConnectionTracker* network_connection_tracker)
    : network_connection_tracker_(network_connection_tracker) {}

NetworkStatusListenerImpl::~NetworkStatusListenerImpl() = default;

void NetworkStatusListenerImpl::Start(
    NetworkStatusListener::Observer* observer) {
  NetworkStatusListener::Start(observer);
  network_connection_tracker_->AddNetworkConnectionObserver(this);
  network_connection_tracker_->GetConnectionType(
      &connection_type_,
      base::BindOnce(&NetworkStatusListenerImpl::OnConnectionChanged,
                     base::Unretained(this)));
}

void NetworkStatusListenerImpl::Stop() {
  network_connection_tracker_->RemoveNetworkConnectionObserver(this);
  NetworkStatusListener::Stop();
}

network::mojom::ConnectionType NetworkStatusListenerImpl::GetConnectionType() {
  return connection_type_;
}

void NetworkStatusListenerImpl::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  DCHECK(observer_);
  connection_type_ = type;
  observer_->OnNetworkChanged(type);
}

}  // namespace download
