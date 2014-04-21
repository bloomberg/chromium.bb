// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/battery/BatteryStatus.h"

#include <limits>

namespace WebCore {

PassRefPtr<BatteryStatus> BatteryStatus::create()
{
    return adoptRef(new BatteryStatus);
}

PassRefPtr<BatteryStatus> BatteryStatus::create(bool charging, double chargingTime, double dischargingTime, double level)
{
    return adoptRef(new BatteryStatus(charging, chargingTime, dischargingTime, level));
}

BatteryStatus::BatteryStatus()
    : m_charging(true)
    , m_chargingTime(0)
    , m_dischargingTime(std::numeric_limits<double>::infinity())
    , m_level(1)
{
}

BatteryStatus::BatteryStatus(bool charging, double chargingTime, double dischargingTime, double level)
    : m_charging(charging)
    , m_chargingTime(chargingTime)
    , m_dischargingTime(dischargingTime)
    , m_level(level)
{
}

} // namespace WebCore

