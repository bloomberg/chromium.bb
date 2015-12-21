// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/heap/BlinkGCMemoryDumpProvider.h"

#include "public/platform/Platform.h"
#include "public/platform/WebProcessMemoryDump.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/Threading.h"

namespace blink {

TEST(BlinkGCDumpProviderTest, MemoryDump)
{
    OwnPtr<WebProcessMemoryDump> dump = adoptPtr(Platform::current()->createProcessMemoryDump());
    ASSERT(dump);
    BlinkGCMemoryDumpProvider::instance()->onMemoryDump(WebMemoryDumpLevelOfDetail::Detailed, dump.get());
    ASSERT(dump->getMemoryAllocatorDump(String::format("blink_gc")));
    ASSERT(dump->getMemoryAllocatorDump(String::format("blink_gc/allocated_objects")));
}

} // namespace blink
