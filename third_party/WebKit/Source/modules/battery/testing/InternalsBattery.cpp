// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/battery/testing/InternalsBattery.h"

#include "modules/battery/BatteryDispatcher.h"

namespace blink {

void InternalsBattery::updateBatteryStatus(Internals&, bool charging, double chargingTime, double dischargingTime, double level)
{
    BatteryDispatcher::instance().OnUpdateBatteryStatus(
        BatteryStatus(charging, chargingTime, dischargingTime, level));
}

} // namespace blink
