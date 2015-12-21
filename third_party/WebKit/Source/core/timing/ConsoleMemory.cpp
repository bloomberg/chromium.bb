// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/timing/ConsoleMemory.h"

#include "core/frame/Console.h"
#include "core/timing/MemoryInfo.h"

namespace blink {

// static
ConsoleMemory& ConsoleMemory::from(Console& console)
{
    ConsoleMemory* supplement = static_cast<ConsoleMemory*>(HeapSupplement<Console>::from(console, supplementName()));
    if (!supplement) {
        supplement = new ConsoleMemory();
        provideTo(console, supplementName(), supplement);
    }
    return *supplement;
}

// static
MemoryInfo* ConsoleMemory::memory(Console& console)
{
    return ConsoleMemory::from(console).memory();
}

MemoryInfo* ConsoleMemory::memory()
{
    return MemoryInfo::create();
}

} // namespace blink
