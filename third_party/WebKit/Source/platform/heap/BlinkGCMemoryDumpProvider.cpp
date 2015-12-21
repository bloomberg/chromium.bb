// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/heap/BlinkGCMemoryDumpProvider.h"

#include "platform/heap/Handle.h"
#include "public/platform/Platform.h"
#include "public/platform/WebMemoryAllocatorDump.h"
#include "public/platform/WebProcessMemoryDump.h"
#include "wtf/StdLibExtras.h"
#include "wtf/Threading.h"

namespace blink {
namespace {

void dumpMemoryTotals(blink::WebProcessMemoryDump* memoryDump)
{
    String dumpName = String::format("blink_gc");
    WebMemoryAllocatorDump* allocatorDump = memoryDump->createMemoryAllocatorDump(dumpName);
    allocatorDump->addScalar("size", "bytes", Heap::allocatedSpace());

    dumpName.append("/allocated_objects");
    WebMemoryAllocatorDump* objectsDump = memoryDump->createMemoryAllocatorDump(dumpName);

    // Heap::markedObjectSize() can be underestimated if we're still in the
    // process of lazy sweeping.
    objectsDump->addScalar("size", "bytes", Heap::allocatedObjectSize() + Heap::markedObjectSize());
}

} // namespace

BlinkGCMemoryDumpProvider* BlinkGCMemoryDumpProvider::instance()
{
    DEFINE_STATIC_LOCAL(BlinkGCMemoryDumpProvider, instance, ());
    return &instance;
}

BlinkGCMemoryDumpProvider::~BlinkGCMemoryDumpProvider()
{
}

bool BlinkGCMemoryDumpProvider::onMemoryDump(WebMemoryDumpLevelOfDetail levelOfDetail, blink::WebProcessMemoryDump* memoryDump)
{
    if (levelOfDetail == WebMemoryDumpLevelOfDetail::Light) {
        dumpMemoryTotals(memoryDump);
        return true;
    }

    Heap::collectGarbage(BlinkGC::NoHeapPointersOnStack, BlinkGC::TakeSnapshot, BlinkGC::ForcedGC);
    dumpMemoryTotals(memoryDump);

    // Merge all dumps collected by Heap::collectGarbage.
    memoryDump->takeAllDumpsFrom(m_currentProcessMemoryDump.get());
    return true;
}

WebMemoryAllocatorDump* BlinkGCMemoryDumpProvider::createMemoryAllocatorDumpForCurrentGC(const String& absoluteName)
{
    return m_currentProcessMemoryDump->createMemoryAllocatorDump(absoluteName);
}

void BlinkGCMemoryDumpProvider::clearProcessDumpForCurrentGC()
{
    m_currentProcessMemoryDump->clear();
}

BlinkGCMemoryDumpProvider::BlinkGCMemoryDumpProvider()
    : m_currentProcessMemoryDump(adoptPtr(Platform::current()->createProcessMemoryDump()))
{
}

} // namespace blink
