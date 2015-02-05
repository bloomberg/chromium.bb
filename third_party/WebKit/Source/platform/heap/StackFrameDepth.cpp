// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/heap/StackFrameDepth.h"

namespace blink {

static const char* s_avoidOptimization = nullptr;

// NEVER_INLINE ensures that |dummy| array on configureLimit() is not optimized away,
// and the stack frame base register is adjusted |kSafeStackFrameSize|.
NEVER_INLINE static uintptr_t currentStackFrameBaseOnCallee(const char* dummy)
{
    s_avoidOptimization = dummy;
    return StackFrameDepth::currentStackFrame();
}

void StackFrameDepth::configureLimit()
{
    // Allocate a large object in stack and query stack frame pointer after it.
    char dummy[kSafeStackFrameSize];
    m_stackFrameLimit = currentStackFrameBaseOnCallee(dummy);

    // Assert that the stack frame can be used.
    dummy[sizeof(dummy) - 1] = 0;
}

} // namespace blink
