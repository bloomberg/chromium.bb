// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StackFrameDepth_h
#define StackFrameDepth_h

#include "platform/PlatformExport.h"
#include "wtf/Assertions.h"
#include <stdint.h>

namespace blink {

// StackFrameDepth keeps track of current call stack frame depth.
// Use isSafeToRecurse() to query if there is a room in current
// call stack for more recursive call.
class PLATFORM_EXPORT StackFrameDepth final {
public:
    inline bool isSafeToRecurse()
    {
        ASSERT(m_stackFrameLimit);

        // Asssume that the stack grows towards lower addresses, which
        // all the ABIs currently supported do.
        //
        // A unit test checks that the assumption holds for a target
        // (HeapTest.StackGrowthDirection.)
        return currentStackFrame() > m_stackFrameLimit;
    }

    static uintptr_t currentStackFrame(const char* dummy = nullptr)
    {
#if COMPILER(GCC)
        return reinterpret_cast<uintptr_t>(__builtin_frame_address(0));
#elif COMPILER(MSVC)
        return reinterpret_cast<uintptr_t>(&dummy) - sizeof(void*);
#else
#error "Stack frame pointer estimation not supported on this platform."
        return 0;
#endif
    }

    void configureLimit();

    static size_t getUnderestimatedStackSize();
    static void* getStackStart();

private:
    // The maximum depth of eager, unrolled trace() calls that is
    // considered safe and allowed.
    static const int kSafeStackFrameSize = 32 * 1024;

    uintptr_t m_stackFrameLimit;
};

} // namespace blink

#endif // StackFrameDepth_h
