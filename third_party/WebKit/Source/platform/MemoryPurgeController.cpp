// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/MemoryPurgeController.h"

#include "platform/TraceEvent.h"
#include "public/platform/Platform.h"
#include "wtf/Partitions.h"

namespace {

// TODO(bashi): Determine appropriate value for this interval.
const size_t kInactiveTimerIntervalInSecond = 10;

} // namespace

namespace blink {

DEFINE_TRACE(MemoryPurgeClient)
{
}

MemoryPurgeController::MemoryPurgeController()
    : m_deviceKind(Platform::current()->isLowEndDeviceMode() ? DeviceKind::LowEnd : DeviceKind::NotSpecified)
    , m_inactiveTimer(this, &MemoryPurgeController::pageInactiveTask)
{
}

DEFINE_EMPTY_DESTRUCTOR_WILL_BE_REMOVED(MemoryPurgeController);

void MemoryPurgeController::pageBecameActive()
{
    m_inactiveTimer.stop();
}

void MemoryPurgeController::pageBecameInactive()
{
    if (!m_inactiveTimer.isActive())
        m_inactiveTimer.startOneShot(kInactiveTimerIntervalInSecond, BLINK_FROM_HERE);
}

void MemoryPurgeController::pageInactiveTask(Timer<MemoryPurgeController>*)
{
    purgeMemory(MemoryPurgeMode::InactiveTab);
}

void MemoryPurgeController::purgeMemory(MemoryPurgeMode purgeMode)
{
    TRACE_EVENT0("blink", "MemoryPurgeController::purgeMemory");
    for (auto& client : m_clients)
        client->purgeMemory(purgeMode, m_deviceKind);
    WTF::Partitions::decommitFreeableMemory();
}

DEFINE_TRACE(MemoryPurgeController)
{
#if ENABLE(OILPAN)
    visitor->trace(m_clients);
#endif
}

} // namespace blink
