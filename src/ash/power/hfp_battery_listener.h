// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_POWER_HFP_BATTERY_LISTENER_H_
#define ASH_POWER_HFP_BATTERY_LISTENER_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "chromeos/audio/cras_audio_handler.h"

namespace device {
class BluetoothAdapter;
}  // namespace device

namespace ash {

// Listens to changes in battery level for devices compatible with the Hands
// Free Profile, updating the corresponding device::BluetoothDevice.
class ASH_EXPORT HfpBatteryListener
    : public chromeos::CrasAudioHandler::AudioObserver {
 public:
  explicit HfpBatteryListener(scoped_refptr<device::BluetoothAdapter> adapter);
  ~HfpBatteryListener() override;

 private:
  friend class HfpBatteryListenerTest;

  // chromeos::CrasAudioHandler::AudioObserver:
  void OnBluetoothBatteryChanged(const std::string& address,
                                 uint32_t level) override;

  scoped_refptr<device::BluetoothAdapter> adapter_;

  DISALLOW_COPY_AND_ASSIGN(HfpBatteryListener);
};

}  // namespace ash

#endif  // ASH_POWER_HFP_BATTERY_LISTENER_H_
