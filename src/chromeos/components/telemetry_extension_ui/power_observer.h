// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_POWER_OBSERVER_H_
#define CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_POWER_OBSERVER_H_

#if defined(OFFICIAL_BUILD)
#error Power observer should only be included in unofficial builds.
#endif

#include "chromeos/components/telemetry_extension_ui/mojom/system_events_service.mojom-forward.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace chromeos {

class PowerObserver : public cros_healthd::mojom::CrosHealthdPowerObserver {
 public:
  PowerObserver();
  PowerObserver(const PowerObserver&) = delete;
  PowerObserver& operator=(const PowerObserver&) = delete;
  ~PowerObserver() override;

  void AddObserver(mojo::PendingRemote<health::mojom::PowerObserver> observer);

  void OnAcInserted() override;
  void OnAcRemoved() override;
  void OnOsSuspend() override;
  void OnOsResume() override;

  // Waits until disconnect handler will be triggered if fake cros_healthd was
  // shutdown.
  void FlushForTesting();

 private:
  void Connect();

  mojo::Receiver<cros_healthd::mojom::CrosHealthdPowerObserver> receiver_;
  mojo::RemoteSet<health::mojom::PowerObserver> observers_;
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_POWER_OBSERVER_H_
