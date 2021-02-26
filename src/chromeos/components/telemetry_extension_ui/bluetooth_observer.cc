// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/telemetry_extension_ui/bluetooth_observer.h"

#include <utility>

#include "base/bind.h"
#include "chromeos/components/telemetry_extension_ui/mojom/system_events_service.mojom.h"
#include "chromeos/services/cros_healthd/public/cpp/service_connection.h"

namespace chromeos {

BluetoothObserver::BluetoothObserver() : receiver_{this} {
  Connect();
}

BluetoothObserver::~BluetoothObserver() = default;

void BluetoothObserver::AddObserver(
    mojo::PendingRemote<health::mojom::BluetoothObserver> observer) {
  health::mojom::BluetoothObserverPtr ptr{std::move(observer)};
  observers_.Add(ptr.PassInterface());
}

void BluetoothObserver::OnAdapterAdded() {
  for (auto& observer : observers_) {
    observer->OnAdapterAdded();
  }
}

void BluetoothObserver::OnAdapterRemoved() {
  for (auto& observer : observers_) {
    observer->OnAdapterRemoved();
  }
}

void BluetoothObserver::OnAdapterPropertyChanged() {
  for (auto& observer : observers_) {
    observer->OnAdapterPropertyChanged();
  }
}

void BluetoothObserver::OnDeviceAdded() {
  for (auto& observer : observers_) {
    observer->OnDeviceAdded();
  }
}

void BluetoothObserver::OnDeviceRemoved() {
  for (auto& observer : observers_) {
    observer->OnDeviceRemoved();
  }
}

void BluetoothObserver::OnDevicePropertyChanged() {
  for (auto& observer : observers_) {
    observer->OnDevicePropertyChanged();
  }
}

void BluetoothObserver::Connect() {
  receiver_.reset();
  cros_healthd::ServiceConnection::GetInstance()->AddBluetoothObserver(
      receiver_.BindNewPipeAndPassRemote());

  // We try to reconnect right after disconnect because Mojo will queue the
  // request and connect to cros_healthd when it becomes available.
  receiver_.set_disconnect_handler(
      base::BindOnce(&BluetoothObserver::Connect, base::Unretained(this)));
}

void BluetoothObserver::FlushForTesting() {
  receiver_.FlushForTesting();
}

}  // namespace chromeos
