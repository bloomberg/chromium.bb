// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/discovery_session.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"

namespace bluetooth {
DiscoverySession::DiscoverySession(
    std::unique_ptr<device::BluetoothDiscoverySession> session)
    : discovery_session_(std::move(session)) {}

DiscoverySession::~DiscoverySession() = default;

void DiscoverySession::IsActive(IsActiveCallback callback) {
  std::move(callback).Run(discovery_session_->IsActive());
}

void DiscoverySession::Stop(StopCallback callback) {
  discovery_session_->Stop();
  std::move(callback).Run(true /* success */);
}

}  // namespace bluetooth
