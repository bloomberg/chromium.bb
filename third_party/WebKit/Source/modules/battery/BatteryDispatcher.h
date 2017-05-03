// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BatteryDispatcher_h
#define BatteryDispatcher_h

#include "core/frame/PlatformEventDispatcher.h"
#include "modules/ModulesExport.h"
#include "modules/battery/BatteryManager.h"
#include "modules/battery/battery_status.h"
#include "services/device/public/interfaces/battery_monitor.mojom-blink.h"

namespace blink {

class MODULES_EXPORT BatteryDispatcher final
    : public GarbageCollectedFinalized<BatteryDispatcher>,
      public PlatformEventDispatcher {
  USING_GARBAGE_COLLECTED_MIXIN(BatteryDispatcher);
  WTF_MAKE_NONCOPYABLE(BatteryDispatcher);

 public:
  static BatteryDispatcher& Instance();

  const BatteryStatus* LatestData() const {
    return has_latest_data_ ? &battery_status_ : nullptr;
  }

 private:
  BatteryDispatcher();

  void QueryNextStatus();
  void OnDidChange(device::mojom::blink::BatteryStatusPtr);
  void UpdateBatteryStatus(const BatteryStatus&);

  // Inherited from PlatformEventDispatcher.
  void StartListening() override;
  void StopListening() override;

  device::mojom::blink::BatteryMonitorPtr monitor_;
  BatteryStatus battery_status_;
  bool has_latest_data_;
};

}  // namespace blink

#endif  // BatteryDispatcher_h
