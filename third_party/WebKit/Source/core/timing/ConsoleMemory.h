// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ConsoleMemory_h
#define ConsoleMemory_h

#include "platform/Supplementable.h"

namespace blink {

class Console;
class MemoryInfo;

class ConsoleMemory final : public NoBaseWillBeGarbageCollectedFinalized<ConsoleMemory>, public WillBeHeapSupplement<Console> {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(ConsoleMemory);
    DECLARE_EMPTY_VIRTUAL_DESTRUCTOR_WILL_BE_REMOVED(ConsoleMemory);
public:
    static ConsoleMemory& from(Console&);
    static MemoryInfo* memory(Console&);

    DECLARE_VIRTUAL_TRACE();

private:
    static const char* supplementName() { return "ConsoleMemory"; }
    MemoryInfo* memory();

    RefPtrWillBeMember<MemoryInfo> m_memory;
};

} // namespace blink

#endif // ConsoleMemory_h
