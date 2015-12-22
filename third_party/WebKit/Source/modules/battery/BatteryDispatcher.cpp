// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/battery/BatteryDispatcher.h"

#include "modules/battery/BatteryStatus.h"
#include "public/platform/Platform.h"

namespace blink {

namespace {

double ensureTwoSignificantDigits(double level)
{
    // Convert battery level value which should be in [0, 1] to a value in [0, 1]
    // with 2 digits of precision. This is to provide a consistent experience
    // across platforms (e.g. on Mac and Android the battery changes are generally
    // reported with 1% granularity). It also serves the purpose of reducing the
    // possibility of fingerprinting and triggers less level change events on
    // platforms where the granularity is high.
    ASSERT(level >= 0 && level <= 1);
    return round(level * 100) / 100.f;
}

} // namespace


BatteryDispatcher& BatteryDispatcher::instance()
{
    DEFINE_STATIC_LOCAL(Persistent<BatteryDispatcher>, batteryDispatcher, (new BatteryDispatcher()));
    return *batteryDispatcher;
}

BatteryDispatcher::BatteryDispatcher()
{
}

BatteryDispatcher::~BatteryDispatcher()
{
}

DEFINE_TRACE(BatteryDispatcher)
{
    visitor->trace(m_batteryStatus);
    PlatformEventDispatcher::trace(visitor);
}

void BatteryDispatcher::updateBatteryStatus(const WebBatteryStatus& batteryStatus)
{
    m_batteryStatus = BatteryStatus::create(
        batteryStatus.charging, batteryStatus.chargingTime, batteryStatus.dischargingTime,
        ensureTwoSignificantDigits(batteryStatus.level));
    notifyControllers();
}

BatteryStatus* BatteryDispatcher::latestData()
{
    return m_batteryStatus.get();
}

void BatteryDispatcher::startListening()
{
    Platform::current()->startListening(WebPlatformEventTypeBattery, this);
}

void BatteryDispatcher::stopListening()
{
    Platform::current()->stopListening(WebPlatformEventTypeBattery);
    m_batteryStatus.clear();
}

} // namespace blink
