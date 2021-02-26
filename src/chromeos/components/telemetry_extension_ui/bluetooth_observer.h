// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_BLUETOOTH_OBSERVER_H_
#define CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_BLUETOOTH_OBSERVER_H_

#if defined(OFFICIAL_BUILD)
#error Bluetooth observer should only be included in unofficial builds.
#endif

#include "chromeos/components/telemetry_extension_ui/mojom/system_events_service.mojom-forward.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace chromeos {

class BluetoothObserver
    : public cros_healthd::mojom::CrosHealthdBluetoothObserver {
 public:
  BluetoothObserver();
  BluetoothObserver(const BluetoothObserver&) = delete;
  BluetoothObserver& operator=(const BluetoothObserver&) = delete;
  ~BluetoothObserver() override;

  void AddObserver(
      mojo::PendingRemote<health::mojom::BluetoothObserver> observer);

  void OnAdapterAdded() override;
  void OnAdapterRemoved() override;
  void OnAdapterPropertyChanged() override;
  void OnDeviceAdded() override;
  void OnDeviceRemoved() override;
  void OnDevicePropertyChanged() override;

  // Waits until disconnect handler will be triggered if fake cros_healthd was
  // shutdown.
  void FlushForTesting();

 private:
  void Connect();

  mojo::Receiver<cros_healthd::mojom::CrosHealthdBluetoothObserver> receiver_;
  mojo::RemoteSet<health::mojom::BluetoothObserver> observers_;
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_BLUETOOTH_OBSERVER_H_
