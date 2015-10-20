// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/MemoryPurgeController.h"

#include "public/platform/Platform.h"

namespace blink {

MemoryPurgeController::MemoryPurgeController()
    : m_deviceKind(Platform::current()->isLowEndDeviceMode() ? DeviceKind::LowEnd : DeviceKind::NotSpecified)
{
}

DEFINE_EMPTY_DESTRUCTOR_WILL_BE_REMOVED(MemoryPurgeController);

void MemoryPurgeController::purgeMemory(MemoryPurgeMode purgeMode)
{
    for (auto& client : m_clients)
        client->purgeMemory(purgeMode, m_deviceKind);
}

DEFINE_TRACE(MemoryPurgeController)
{
#if ENABLE(OILPAN)
    visitor->trace(m_clients);
#endif
}

} // namespace blink
