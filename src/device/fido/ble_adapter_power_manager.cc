// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/ble_adapter_power_manager.h"

#include <utility>

#include "base/bind_helpers.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

namespace device {

BleAdapterPowerManager::BleAdapterPowerManager(
    FidoRequestHandlerBase* request_handler)
    : request_handler_(request_handler), weak_factory_(this) {
  BluetoothAdapterFactory::Get().GetAdapter(base::BindRepeating(
      &BleAdapterPowerManager::Start, weak_factory_.GetWeakPtr()));
}

BleAdapterPowerManager::~BleAdapterPowerManager() {
  if (adapter_powered_on_programmatically_)
    SetAdapterPower(false /* set_power_on */);

  if (adapter_)
    adapter_->RemoveObserver(this);
}

void BleAdapterPowerManager::SetAdapterPower(bool set_power_on) {
  if (set_power_on)
    adapter_powered_on_programmatically_ = true;

  adapter_->SetPowered(set_power_on, base::DoNothing(), base::DoNothing());
}

void BleAdapterPowerManager::AdapterPoweredChanged(BluetoothAdapter* adapter,
                                                   bool powered) {
  request_handler_->OnBluetoothAdapterPowerChanged(powered);
}

void BleAdapterPowerManager::Start(scoped_refptr<BluetoothAdapter> adapter) {
  DCHECK(!adapter_);
  adapter_ = std::move(adapter);
  DCHECK(adapter_);
  adapter_->AddObserver(this);

  request_handler_->OnBluetoothAdapterEnumerated(
      adapter_->IsPresent(), adapter_->IsPowered(), adapter_->CanPower());
}

}  // namespace device
