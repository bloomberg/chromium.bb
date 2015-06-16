// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/heap/BlinkGCMemoryDumpProvider.h"

#include "public/platform/Platform.h"
#include "public/platform/WebProcessMemoryDump.h"

#include <gtest/gtest.h>

namespace blink {

TEST(BlinkGCDumpProviderTest, MemoryDump)
{
    WebProcessMemoryDump* dump  = Platform::current()->createProcessMemoryDump();
    ASSERT(dump);
    BlinkGCMemoryDumpProvider::instance()->onMemoryDump(dump);
    ASSERT(dump->getMemoryAllocatorDump("blink_gc"));
    ASSERT(dump->getMemoryAllocatorDump("blink_gc/allocated_objects"));
}

} // namespace blink
