// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/MemoryPurgeController.h"

#include "base/sys_info.h"
#include "platform/TraceEvent.h"
#include "platform/graphics/ImageDecodingStore.h"
#include "public/platform/Platform.h"
#include "wtf/Partitions.h"

namespace blink {

void MemoryPurgeController::onMemoryPressure(WebMemoryPressureLevel level)
{
    if (level == WebMemoryPressureLevelCritical) {
        // Clear the image cache.
        ImageDecodingStore::instance().clear();
    }
}

DEFINE_TRACE(MemoryPurgeClient)
{
}

MemoryPurgeController::MemoryPurgeController()
    : m_deviceKind(base::SysInfo::IsLowEndDevice() ? DeviceKind::LowEnd : DeviceKind::NotSpecified)
{
}

MemoryPurgeController::~MemoryPurgeController()
{
}

void MemoryPurgeController::purgeMemory()
{
    // TODO(bashi): Add UMA
    TRACE_EVENT0("blink", "MemoryPurgeController::purgeMemory");
    for (auto& client : m_clients)
        client->purgeMemory(m_deviceKind);
    WTF::Partitions::decommitFreeableMemory();
}

DEFINE_TRACE(MemoryPurgeController)
{
#if ENABLE(OILPAN)
    visitor->trace(m_clients);
#endif
}

} // namespace blink
