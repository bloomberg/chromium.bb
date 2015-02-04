// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/heap/StackFrameDepth.h"

#if OS(WIN)
#include <stddef.h>
#include <windows.h>
#include <winnt.h>
#elif defined(__GLIBC__)
extern "C" void* __libc_stack_end;  // NOLINT
#endif

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

size_t StackFrameDepth::getUnderestimatedStackSize()
{
#if defined(__GLIBC__) || OS(ANDROID) || OS(FREEBSD)
    // We cannot get the stack size in these platforms because
    // pthread_getattr_np() can fail for the main thread.
    // This is OK because ThreadState::current() doesn't use the stack size
    // in these platforms.
    return 0;
#elif OS(MACOSX)
    return pthread_get_stacksize_np(pthread_self());
#elif OS(WIN) && COMPILER(MSVC)
    // On Windows stack limits for the current thread are available in
    // the thread information block (TIB). Its fields can be accessed through
    // FS segment register on x86 and GS segment register on x86_64.
#ifdef _WIN64
    return __readgsqword(offsetof(NT_TIB64, StackBase)) - __readgsqword(offsetof(NT_TIB64, StackLimit));
#else
    return __readfsdword(offsetof(NT_TIB, StackBase)) - __readfsdword(offsetof(NT_TIB, StackLimit));
#endif
#else
    return 0;
#endif
}

} // namespace blink
