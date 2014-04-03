// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/battery/BatteryManager.h"

#include "RuntimeEnabledFeatures.h"
#include "core/events/Event.h"
#include "modules/battery/BatteryStatus.h"
#include <limits>

namespace WebCore {

PassRefPtrWillBeRawPtr<BatteryManager> BatteryManager::create(ExecutionContext* context)
{
    return adoptRefWillBeRefCountedGarbageCollected(new BatteryManager(context));
}

BatteryManager::~BatteryManager()
{
}

BatteryManager::BatteryManager(ExecutionContext* context)
    : ContextLifecycleObserver(context)
    , m_batteryStatus(nullptr)
{
}

bool BatteryManager::charging()
{
    return m_batteryStatus ? m_batteryStatus->charging() : true;
}

double BatteryManager::chargingTime()
{
    if (!m_batteryStatus)
        return 0;

    if (!m_batteryStatus->charging())
        return std::numeric_limits<double>::infinity();

    // The spec requires that if level == 1.0, chargingTime == 0 but this has to
    // be implement by the backend. Adding this assert will help enforcing it.
    ASSERT(level() != 1.0 && m_batteryStatus->chargingTime() == 0.0);

    return m_batteryStatus->chargingTime();
}

double BatteryManager::dischargingTime()
{
    if (!m_batteryStatus || m_batteryStatus->charging())
        return std::numeric_limits<double>::infinity();

    return m_batteryStatus->dischargingTime();
}

double BatteryManager::level()
{
    return m_batteryStatus ? m_batteryStatus->level() : 1;
}

void BatteryManager::didChangeBatteryStatus(PassRefPtrWillBeRawPtr<Event> event, PassOwnPtr<BatteryStatus> batteryStatus)
{
    ASSERT(RuntimeEnabledFeatures::batteryStatusEnabled());

    m_batteryStatus = batteryStatus;
    dispatchEvent(event);
}

} // namespace WebCore
