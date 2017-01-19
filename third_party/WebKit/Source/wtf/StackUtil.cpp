// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wtf/StackUtil.h"

#include "wtf/Assertions.h"
#include "wtf/Threading.h"
#include "wtf/WTFThreadData.h"

#if OS(WIN)
#include <stddef.h>
#include <windows.h>
#include <winnt.h>
#elif defined(__GLIBC__)
extern "C" void* __libc_stack_end;  // NOLINT
#endif

namespace WTF {

size_t getUnderestimatedStackSize() {
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
  // If so, a conservative size estimate is returned.

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

  // Return a 512k stack size, (conservatively) assuming the following:
  //  - that size is much lower than the pthreads default (x86 pthreads has a 2M
  //    default.)
  //  - no one is running Blink with an RLIMIT_STACK override, let alone as
  //    low as 512k.
  //
  return 512 * 1024;
#elif OS(MACOSX)
  // pthread_get_stacksize_np() returns too low a value for the main thread on
  // OSX 10.9,
  // http://mail.openjdk.java.net/pipermail/hotspot-dev/2013-October/011369.html
  //
  // Multiple workarounds possible, adopt the one made by
  // https://github.com/robovm/robovm/issues/274
  // (cf.
  // https://developer.apple.com/library/mac/documentation/Cocoa/Conceptual/Multithreading/CreatingThreads/CreatingThreads.html
  // on why hardcoding sizes is reasonable.)
  if (pthread_main_np()) {
#if defined(IOS)
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    size_t guardSize = 0;
    pthread_attr_getguardsize(&attr, &guardSize);
    // Stack size for the main thread is 1MB on iOS including the guard page
    // size.
    return (1 * 1024 * 1024 - guardSize);
#else
    // Stack size for the main thread is 8MB on OSX excluding the guard page
    // size.
    return (8 * 1024 * 1024);
#endif
  }
  return pthread_get_stacksize_np(pthread_self());
#elif OS(WIN) && COMPILER(MSVC)
  return WTFThreadData::threadStackSize();
#else
#error "Stack frame size estimation not supported on this platform."
  return 0;
#endif
}

void* getStackStart() {
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
  NOTREACHED();
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

namespace internal {

uintptr_t mainThreadUnderestimatedStackSize() {
  size_t underestimatedStackSize = getUnderestimatedStackSize();
  // See comment in mayNotBeMainThread as to why we subtract here.
  if (underestimatedStackSize > sizeof(void*)) {
    underestimatedStackSize = underestimatedStackSize - sizeof(void*);
  }
  return underestimatedStackSize;
}

#if OS(WIN) && COMPILER(MSVC)
size_t threadStackSize() {
  // Notice that we cannot use the TIB's StackLimit for the stack end, as i
  // tracks the end of the committed range. We're after the end of the reserved
  // stack area (most of which will be uncommitted, most times.)
  MEMORY_BASIC_INFORMATION stackInfo;
  memset(&stackInfo, 0, sizeof(MEMORY_BASIC_INFORMATION));
  size_t resultSize =
      VirtualQuery(&stackInfo, &stackInfo, sizeof(MEMORY_BASIC_INFORMATION));
  DCHECK_GE(resultSize, sizeof(MEMORY_BASIC_INFORMATION));
  uint8_t* stackEnd = reinterpret_cast<uint8_t*>(stackInfo.AllocationBase);

  uint8_t* stackStart = reinterpret_cast<uint8_t*>(WTF::getStackStart());
  RELEASE_ASSERT(stackStart && stackStart > stackEnd);
  size_t s_threadStackSize = static_cast<size_t>(stackStart - stackEnd);
  // When the third last page of the reserved stack is accessed as a
  // guard page, the second last page will be committed (along with removing
  // the guard bit on the third last) _and_ a stack overflow exception
  // is raised.
  //
  // We have zero interest in running into stack overflow exceptions while
  // marking objects, so simply consider the last three pages + one above
  // as off-limits and adjust the reported stack size accordingly.
  //
  // http://blogs.msdn.com/b/satyem/archive/2012/08/13/thread-s-stack-memory-management.aspx
  // explains the details.
  RELEASE_ASSERT(s_threadStackSize > 4 * 0x1000);
  s_threadStackSize -= 4 * 0x1000;
  return s_threadStackSize;
}
#endif

}  // namespace internal

}  // namespace WTF
