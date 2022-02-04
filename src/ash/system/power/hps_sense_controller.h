// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_HPS_SENSE_CONTROLLER_H_
#define ASH_SYSTEM_POWER_HPS_SENSE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/dbus/hps/hps_dbus_client.h"
#include "chromeos/dbus/hps/hps_service.pb.h"

namespace ash {

// Helper class for chromeos::HpsDBusClient, responsible for enabling/disabling
// HPS via the client and is responsible for maintaining state between restarts.
class ASH_EXPORT HpsSenseController : public chromeos::HpsDBusClient::Observer {
 public:
  HpsSenseController();
  HpsSenseController(const HpsSenseController&) = delete;
  HpsSenseController& operator=(const HpsSenseController&) = delete;

  ~HpsSenseController() override;

  // Enables the HpsSense feature inside HpsDBusClient; and it only sends the
  // method call if HpsSense is not enabled yet.
  void EnableHpsSense();
  // Disables the HpsSense feature inside HpsDBusClient if it is currently
  // enabled.
  void DisableHpsSense();

  // chromeos::HpsDBusClient::Observer:
  void OnHpsNotifyChanged(hps::HpsResult state) override;
  // Re-enables HpsSense on restart if it was enabled before.
  void OnRestart() override;
  void OnShutdown() override;

 private:
  // Callback used when the Hps Service is available.
  void OnHpsServiceAvailable(bool service_is_avaible);

  bool is_hps_sense_enabled = false;

  base::ScopedObservation<chromeos::HpsDBusClient,
                          chromeos::HpsDBusClient::Observer>
      hps_observation_{this};
  base::WeakPtrFactory<HpsSenseController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_PERIPHERAL_BATTERY_LISTENER_H_