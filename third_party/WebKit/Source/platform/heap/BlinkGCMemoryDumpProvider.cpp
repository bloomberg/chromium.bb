// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "Source/platform/heap/BlinkGCMemoryDumpProvider.h"

#include "platform/heap/Handle.h"
#include "public/platform/WebMemoryAllocatorDump.h"
#include "public/platform/WebProcessMemoryDump.h"
#include "wtf/StdLibExtras.h"

namespace blink {

BlinkGCMemoryDumpProvider* BlinkGCMemoryDumpProvider::instance()
{
    DEFINE_STATIC_LOCAL(BlinkGCMemoryDumpProvider, instance, ());
    return &instance;
}

bool BlinkGCMemoryDumpProvider::onMemoryDump(blink::WebProcessMemoryDump* memoryDump)
{
    WebMemoryAllocatorDump* allocatorDump = memoryDump->createMemoryAllocatorDump("blink_gc");
    allocatorDump->AddScalar("size", "bytes", Heap::allocatedSpace());

    WebMemoryAllocatorDump* objectsDump = memoryDump->createMemoryAllocatorDump("blink_gc/allocated_objects");
    objectsDump->AddScalar("size", "bytes", Heap::allocatedObjectSize());
    objectsDump->AddScalar("estimated_live_object_size", "bytes", Heap::estimatedLiveObjectSize());

    return true;
}

BlinkGCMemoryDumpProvider::BlinkGCMemoryDumpProvider()
{
}

BlinkGCMemoryDumpProvider::~BlinkGCMemoryDumpProvider()
{
}

} // namespace blink
