// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/battery/BatteryDispatcher.h"

#include "platform/threading/BindForMojo.h"
#include "public/platform/Platform.h"
#include "wtf/Assertions.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

BatteryDispatcher& BatteryDispatcher::instance()
{
    DEFINE_STATIC_LOCAL(Persistent<BatteryDispatcher>, batteryDispatcher, (new BatteryDispatcher()));
    return *batteryDispatcher;
}

BatteryDispatcher::BatteryDispatcher()
    : m_hasLatestData(false)
{
}

void BatteryDispatcher::queryNextStatus()
{
    m_monitor->QueryNextStatus(
        sameThreadBindForMojo(&BatteryDispatcher::onDidChange, this));
}

void BatteryDispatcher::onDidChange(device::BatteryStatusPtr batteryStatus)
{
    // m_monitor can be null during testing.
    if (m_monitor)
        queryNextStatus();

    ASSERT(batteryStatus);

    updateBatteryStatus(BatteryStatus(
        batteryStatus->charging,
        batteryStatus->charging_time,
        batteryStatus->discharging_time,
        batteryStatus->level));
}

void BatteryDispatcher::updateBatteryStatus(const BatteryStatus& batteryStatus)
{
    m_batteryStatus = batteryStatus;
    m_hasLatestData = true;
    notifyControllers();
}

void BatteryDispatcher::startListening()
{
    ASSERT(!m_monitor.is_bound());
    Platform::current()->connectToRemoteService(mojo::GetProxy(&m_monitor));
    // m_monitor can be null during testing.
    if (m_monitor)
        queryNextStatus();
}

void BatteryDispatcher::stopListening()
{
    // m_monitor can be null during testing.
    if (m_monitor)
        m_monitor.reset();
    m_hasLatestData = false;
}

} // namespace blink
