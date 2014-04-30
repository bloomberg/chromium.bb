// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/battery/BatteryManager.h"

#include "RuntimeEnabledFeatures.h"
#include "core/events/Event.h"
#include "modules/battery/BatteryDispatcher.h"
#include "modules/battery/BatteryStatus.h"
#include <limits>

namespace WebCore {

PassRefPtrWillBeRawPtr<BatteryManager> BatteryManager::create(ExecutionContext* context)
{
    RefPtrWillBeRawPtr<BatteryManager> batteryManager(adoptRefWillBeRefCountedGarbageCollected(new BatteryManager(context)));
    batteryManager->suspendIfNeeded();
    return batteryManager.release();
}

BatteryManager::~BatteryManager()
{
}

BatteryManager::BatteryManager(ExecutionContext* context)
    : ActiveDOMObject(context)
    , DeviceSensorEventController(toDocument(context)->page())
{
    m_hasEventListener = true;
    startUpdating();
}

bool BatteryManager::charging()
{
    if (const BatteryStatus* lastData = BatteryDispatcher::instance().getLatestData())
        return lastData->charging();

    return true;
}

double BatteryManager::chargingTime()
{
    if (const BatteryStatus* lastData = BatteryDispatcher::instance().getLatestData())
        return lastData->chargingTime();

    return 0;
}

double BatteryManager::dischargingTime()
{
    if (const BatteryStatus* lastData = BatteryDispatcher::instance().getLatestData())
        return lastData->dischargingTime();

    return std::numeric_limits<double>::infinity();
}

double BatteryManager::level()
{
    if (const BatteryStatus* lastData = BatteryDispatcher::instance().getLatestData())
        return lastData->level();

    return 1;
}

void BatteryManager::didChangeBatteryStatus(PassRefPtrWillBeRawPtr<Event> event)
{
    ASSERT(RuntimeEnabledFeatures::batteryStatusEnabled());

    dispatchEvent(event);
}

void BatteryManager::registerWithDispatcher()
{
    BatteryDispatcher::instance().addClient(this);
}

void BatteryManager::unregisterWithDispatcher()
{
    BatteryDispatcher::instance().removeClient(this);
}

bool BatteryManager::hasLastData()
{
    return false;
}

PassRefPtrWillBeRawPtr<Event> BatteryManager::getLastEvent()
{
    // Events are dispached via BatteryManager::didChangeBatteryStatus()
    return nullptr;
}

bool BatteryManager::isNullEvent(Event*)
{
    return false;
}

Document* BatteryManager::document()
{
    return toDocument(executionContext());
}

void BatteryManager::suspend()
{
    m_hasEventListener = false;
    stopUpdating();
}

void BatteryManager::resume()
{
    m_hasEventListener = true;
    startUpdating();
}

void BatteryManager::stop()
{
    m_hasEventListener = false;
    stopUpdating();
}

} // namespace WebCore
