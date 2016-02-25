// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/battery/BatteryDispatcher.h"

#include "wtf/PassOwnPtr.h"

namespace blink {

BatteryDispatcher& BatteryDispatcher::instance()
{
    DEFINE_STATIC_LOCAL(Persistent<BatteryDispatcher>, batteryDispatcher, (new BatteryDispatcher()));
    return *batteryDispatcher;
}

BatteryDispatcher::BatteryDispatcher()
    : m_hasLatestData(false)
    , m_batteryDispatcherProxy(adoptPtr(new BatteryDispatcherProxy(this)))
{
}

BatteryDispatcher::~BatteryDispatcher()
{
}

void BatteryDispatcher::OnUpdateBatteryStatus(const BatteryStatus& batteryStatus)
{
    m_batteryStatus = batteryStatus;
    m_hasLatestData = true;
    notifyControllers();
}

void BatteryDispatcher::startListening()
{
    m_batteryDispatcherProxy->StartListening();
}

void BatteryDispatcher::stopListening()
{
    m_batteryDispatcherProxy->StopListening();
    m_hasLatestData = false;
}

} // namespace blink
