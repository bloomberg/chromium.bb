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

PassRefPtr<BatteryManager> BatteryManager::create(ExecutionContext* context)
{
    return adoptRef(new BatteryManager(context));
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
    if (!m_batteryStatus || !m_batteryStatus->charging())
        return std::numeric_limits<double>::infinity();

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

void BatteryManager::didChangeBatteryStatus(PassRefPtr<Event> event, PassOwnPtr<BatteryStatus> batteryStatus)
{
    ASSERT(RuntimeEnabledFeatures::batteryStatusEnabled());

    m_batteryStatus = batteryStatus;
    dispatchEvent(event);
}

} // namespace WebCore
