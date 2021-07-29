// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_SCANNING_SCANNER_BROKER_H_
#define ASH_QUICK_PAIR_SCANNING_SCANNER_BROKER_H_

#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/protocol.h"
#include "base/observer_list_types.h"

namespace ash {
namespace quick_pair {

// The ScannerBroker is the entry point for the Scanning component in the Quick
// Pair system. It is responsible for brokering the start/stop scanning calls
// to the correct concrete Scanner implementation, and exposing an observer
// pattern for other components to become aware of device found/lost events.
class ScannerBroker {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnDeviceFound(const Device& device) = 0;
    virtual void OnDeviceLost(const Device& device) = 0;
  };

  virtual ~ScannerBroker() = default;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
  virtual void StartScanning(Protocol protocol) = 0;
  virtual void StopScanning(Protocol protocol) = 0;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_SCANNING_SCANNER_BROKER_H_
