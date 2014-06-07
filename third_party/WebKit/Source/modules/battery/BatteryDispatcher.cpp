// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/battery/BatteryDispatcher.h"

#include "modules/battery/BatteryStatus.h"
#include "platform/NotImplemented.h"
#include "public/platform/Platform.h"

namespace WebCore {

BatteryDispatcher& BatteryDispatcher::instance()
{
    DEFINE_STATIC_LOCAL(BatteryDispatcher, batteryDispatcher, ());
    return batteryDispatcher;
}

BatteryDispatcher::BatteryDispatcher()
{
}

BatteryDispatcher::~BatteryDispatcher()
{
}

void BatteryDispatcher::addClient(BatteryManager* batteryManager)
{
    addController(batteryManager);
}

void BatteryDispatcher::removeClient(BatteryManager* batteryManager)
{
    removeController(batteryManager);
}

void BatteryDispatcher::updateBatteryStatus(const blink::WebBatteryStatus& batteryStatus)
{
    RefPtr<BatteryStatus> oldStatus = m_batteryStatus;
    m_batteryStatus = BatteryStatus::create(batteryStatus.charging, batteryStatus.chargingTime, batteryStatus.dischargingTime, batteryStatus.level);

    if (oldStatus) {
        if (m_batteryStatus->charging() != oldStatus->charging())
            didChangeBatteryStatus(EventTypeNames::chargingchange);
        if (m_batteryStatus->charging() && m_batteryStatus->chargingTime() != oldStatus->chargingTime())
            didChangeBatteryStatus(EventTypeNames::chargingtimechange);
        if (!m_batteryStatus->charging() && m_batteryStatus->dischargingTime() != oldStatus->dischargingTime())
            didChangeBatteryStatus(EventTypeNames::dischargingtimechange);
        if (m_batteryStatus->level() != oldStatus->level())
            didChangeBatteryStatus(EventTypeNames::levelchange);
    } else {
        // There was no previous state.
        didChangeBatteryStatus(EventTypeNames::chargingchange);
        didChangeBatteryStatus(EventTypeNames::chargingtimechange);
        didChangeBatteryStatus(EventTypeNames::dischargingtimechange);
        didChangeBatteryStatus(EventTypeNames::levelchange);
    }

}

const BatteryStatus* BatteryDispatcher::getLatestData()
{
    return m_batteryStatus.get();
}

void BatteryDispatcher::didChangeBatteryStatus(const AtomicString& eventType)
{
    RefPtrWillBeRawPtr<Event> event = Event::create(eventType);

    {
        TemporaryChange<bool> changeIsDispatching(m_isDispatching, true);
        // Don't fire listeners removed or added during event dispatch.
        size_t size = m_controllers.size();
        for (size_t i = 0; i < size; ++i) {
            if (m_controllers[i])
                static_cast<BatteryManager*>(m_controllers[i])->didChangeBatteryStatus(event);
        }
    }

    if (m_needsPurge)
        purgeControllers();
}

void BatteryDispatcher::startListening()
{
    blink::Platform::current()->setBatteryStatusListener(this);
}

void BatteryDispatcher::stopListening()
{
    blink::Platform::current()->setBatteryStatusListener(0);
}

}
