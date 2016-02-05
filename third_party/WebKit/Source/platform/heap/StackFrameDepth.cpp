// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/heap/StackFrameDepth.h"

#include "public/platform/Platform.h"

#if OS(WIN)
#include <stddef.h>
#include <windows.h>
#include <winnt.h>
#elif defined(__GLIBC__)
extern "C" void* __libc_stack_end;  // NOLINT
#endif

namespace blink {

static const char* s_avoidOptimization = nullptr;

uintptr_t StackFrameDepth::s_stackFrameLimit = 0;
#if ENABLE(ASSERT)
bool StackFrameDepth::s_isEnabled = false;
bool StackFrameDepth::s_isUsingFallbackStackSize = false;
#endif

// NEVER_INLINE ensures that |dummy| array on configureLimit() is not optimized away,
// and the stack frame base register is adjusted |kSafeStackFrameSize|.
NEVER_INLINE static uintptr_t currentStackFrameBaseOnCallee(const char* dummy)
{
    s_avoidOptimization = dummy;
    return StackFrameDepth::currentStackFrame();
}

uintptr_t StackFrameDepth::getFallbackStackLimit()
{
    // Allocate an |kSafeStackFrameSize|-sized object on stack and query
    // stack frame base after it.
    char dummy[kSafeStackFrameSize];

    // Check that the stack frame can be used.
    dummy[sizeof(dummy) - 1] = 0;
#if ENABLE(ASSERT)
    // Use a larger stack limit for what's acceptable if the platform
    // thread ends up using the fallback size to decide if switching to
    // lazy marking is in order.
    s_isUsingFallbackStackSize = true;
#endif
    return currentStackFrameBaseOnCallee(dummy);
}

void StackFrameDepth::enableStackLimit()
{
#if ENABLE(ASSERT)
    s_isEnabled = true;
    s_isUsingFallbackStackSize = false;
#endif

    // Windows and OSX platforms will always return a non-zero estimate.
    size_t stackSize = getUnderestimatedStackSize();
    if (!stackSize) {
        s_stackFrameLimit = getFallbackStackLimit();
        return;
    }

    static const int kStackRoomSize = 1024;

    Address stackBase = reinterpret_cast<Address>(getStackStart());
    RELEASE_ASSERT(stackSize > static_cast<const size_t>(kStackRoomSize));
    size_t stackRoom = stackSize - kStackRoomSize;
    RELEASE_ASSERT(stackBase > reinterpret_cast<Address>(stackRoom));
    s_stackFrameLimit = reinterpret_cast<uintptr_t>(stackBase - stackRoom);
}

size_t StackFrameDepth::getUnderestimatedStackSize()
{
    // FIXME: ASAN bot uses a fake stack as a thread stack frame,
    // and its size is different from the value which APIs tells us.
#if defined(ADDRESS_SANITIZER)
    return 0;
#endif

    // FIXME: On Mac OSX and Linux, this method cannot estimate stack size
    // correctly for the main thread.

#if defined(__GLIBC__) || OS(ANDROID) || OS(FREEBSD)
    // pthread_getattr_np() can fail if the thread is not invoked by
    // pthread_create() (e.g., the main thread of webkit_unit_tests).
    // In this case, this method returns 0 and the caller must handle it.

    pthread_attr_t attr;
    int error;
#if OS(FREEBSD)
    pthread_attr_init(&attr);
    error = pthread_attr_get_np(pthread_self(), &attr);
#else
    error = pthread_getattr_np(pthread_self(), &attr);
#endif
    if (!error) {
        void* base;
        size_t size;
        error = pthread_attr_getstack(&attr, &base, &size);
        RELEASE_ASSERT(!error);
        pthread_attr_destroy(&attr);
        return size;
    }
#if OS(FREEBSD)
    pthread_attr_destroy(&attr);
#endif

    return 0;
#elif OS(MACOSX)
    // pthread_get_stacksize_np() returns too low a value for the main thread on
    // OSX 10.9, http://mail.openjdk.java.net/pipermail/hotspot-dev/2013-October/011369.html
    //
    // Multiple workarounds possible, adopt the one made by https://github.com/robovm/robovm/issues/274
    // (cf. https://developer.apple.com/library/mac/documentation/Cocoa/Conceptual/Multithreading/CreatingThreads/CreatingThreads.html
    // on why hardcoding sizes is reasonable.)
    if (pthread_main_np()) {
#if defined(IOS)
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        size_t guardSize = 0;
        pthread_attr_getguardsize(&attr, &guardSize);
        // Stack size for the main thread is 1MB on iOS including the guard page size.
        return (1 * 1024 * 1024 - guardSize);
#else
        // Stack size for the main thread is 8MB on OSX excluding the guard page size.
        return (8 * 1024 * 1024);
#endif
    }
    return pthread_get_stacksize_np(pthread_self());
#elif OS(WIN) && COMPILER(MSVC)
    return ThreadState::current()->threadStackSize();
#else
#error "Stack frame size estimation not supported on this platform."
    return 0;
#endif
}

void* StackFrameDepth::getStackStart()
{
#if defined(__GLIBC__) || OS(ANDROID) || OS(FREEBSD)
    pthread_attr_t attr;
    int error;
#if OS(FREEBSD)
    pthread_attr_init(&attr);
    error = pthread_attr_get_np(pthread_self(), &attr);
#else
    error = pthread_getattr_np(pthread_self(), &attr);
#endif
    if (!error) {
        void* base;
        size_t size;
        error = pthread_attr_getstack(&attr, &base, &size);
        RELEASE_ASSERT(!error);
        pthread_attr_destroy(&attr);
        return reinterpret_cast<uint8_t*>(base) + size;
    }
#if OS(FREEBSD)
    pthread_attr_destroy(&attr);
#endif
#if defined(__GLIBC__)
    // pthread_getattr_np can fail for the main thread. In this case
    // just like NaCl we rely on the __libc_stack_end to give us
    // the start of the stack.
    // See https://code.google.com/p/nativeclient/issues/detail?id=3431.
    return __libc_stack_end;
#else
    ASSERT_NOT_REACHED();
    return nullptr;
#endif
#elif OS(MACOSX)
    return pthread_get_stackaddr_np(pthread_self());
#elif OS(WIN) && COMPILER(MSVC)
    // On Windows stack limits for the current thread are available in
    // the thread information block (TIB). Its fields can be accessed through
    // FS segment register on x86 and GS segment register on x86_64.
#ifdef _WIN64
    return reinterpret_cast<void*>(__readgsqword(offsetof(NT_TIB64, StackBase)));
#else
    return reinterpret_cast<void*>(__readfsdword(offsetof(NT_TIB, StackBase)));
#endif
#else
#error Unsupported getStackStart on this platform.
#endif
}

} // namespace blink
