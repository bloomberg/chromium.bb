// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/virtual_discovery.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread_task_runner_handle.h"
#include "device/fido/fido_device.h"
#include "device/fido/fido_device_discovery.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/virtual_fido_device_authenticator.h"

namespace content {

VirtualFidoDiscovery::VirtualFidoDiscovery(
    ::device::FidoTransportProtocol transport)
    : FidoDeviceDiscovery(transport) {}

VirtualFidoDiscovery::~VirtualFidoDiscovery() = default;

void VirtualFidoDiscovery::AddVirtualDevice(
    std::unique_ptr<device::VirtualFidoDevice> device) {
  // The real implementation would never notify the client's observer about
  // devices before the client calls Start(), mimic the same behavior.
  if (is_start_requested()) {
    auto authenticator =
        std::make_unique<device::VirtualFidoDeviceAuthenticator>(
            std::move(device));
    FidoDeviceDiscovery::AddAuthenticator(std::move(authenticator));
  } else {
    devices_pending_discovery_start_.push_back(std::move(device));
  }
}

bool VirtualFidoDiscovery::RemoveVirtualDevice(base::StringPiece device_id) {
  DCHECK(is_start_requested());
  return ::device::FidoDeviceDiscovery::RemoveDevice(device_id);
}

void VirtualFidoDiscovery::StartInternal() {
  for (auto& device : devices_pending_discovery_start_) {
    auto authenticator =
        std::make_unique<device::VirtualFidoDeviceAuthenticator>(
            std::move(device));
    FidoDeviceDiscovery::AddAuthenticator(std::move(authenticator));
  }
  devices_pending_discovery_start_.clear();

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&VirtualFidoDiscovery::NotifyDiscoveryStarted,
                                AsWeakPtr(), true /* success */));
}

}  // namespace content
