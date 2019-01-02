// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/threading.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/platform/wtf/date_math.h"
#include "third_party/blink/renderer/platform/wtf/dtoa/double-conversion.h"
#include "third_party/blink/renderer/platform/wtf/wtf_thread_data.h"

#if defined(OS_WIN)
#include <windows.h>
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
#include <pthread.h>
#else
#error Blink does not support threading on your platform.
#endif

#if defined(OS_LINUX)
#include <sys/syscall.h>
#elif defined(OS_ANDROID)
#include <sys/types.h>
#endif

namespace WTF {

// Current thread identity

namespace internal {

ThreadIdentifier CurrentThreadSyscall() {
#if defined(OS_WIN)
  return static_cast<ThreadIdentifier>(GetCurrentThreadId());
#elif defined(OS_MACOSX)
  return pthread_mach_thread_np(pthread_self());
#elif defined(OS_LINUX)
  return syscall(__NR_gettid);
#elif defined(OS_ANDROID)
  return gettid();
#else
  return reinterpret_cast<uintptr_t>(pthread_self());
#endif
}

}  // namespace internal

namespace {
bool g_current_thread_key_initialized = false;

#if defined(OS_WIN)
DWORD g_current_thread_key;
void RawCurrentThreadInit() {
  g_current_thread_key = ::TlsAlloc();
  CHECK_NE(g_current_thread_key, TLS_OUT_OF_INDEXES);
}
void* RawCurrentThreadGet() {
  return ::TlsGetValue(g_current_thread_key);
}
void RawCurrentThreadSet(void* value) {
  ::TlsSetValue(g_current_thread_key, value);
}
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
pthread_key_t g_current_thread_key;
void RawCurrentThreadInit() {
  int error = pthread_key_create(&g_current_thread_key, nullptr);
  CHECK(!error);
}
void* RawCurrentThreadGet() {
  return pthread_getspecific(g_current_thread_key);
}
void RawCurrentThreadSet(void* value) {
  pthread_setspecific(g_current_thread_key, value);
}
#endif
}  // namespace

void InitializeCurrentThread() {
  DCHECK(!g_current_thread_key_initialized);
  RawCurrentThreadInit();
  g_current_thread_key_initialized = true;
}

ThreadIdentifier CurrentThread() {
  // This doesn't use WTF::ThreadSpecific (e.g. WTFThreadData) because
  // ThreadSpecific now depends on currentThread. It is necessary to avoid this
  // or a similar loop:
  //
  // CurrentThread
  // -> WtfThreadData
  // -> ThreadSpecific::operator*
  // -> IsMainThread
  // -> CurrentThread
  static_assert(sizeof(ThreadIdentifier) <= sizeof(void*),
                "ThreadIdentifier must fit in a void*.");
  DCHECK(g_current_thread_key_initialized);
  void* value = RawCurrentThreadGet();
  if (UNLIKELY(!value)) {
    value = reinterpret_cast<void*>(internal::CurrentThreadSyscall());
    DCHECK(value);
    RawCurrentThreadSet(value);
  }
  return reinterpret_cast<ThreadIdentifier>(value);
}

// For debugging only -- whether a non-main thread has been created.
// No synchronization is required, since this is called before any such thread
// exists.

#if DCHECK_IS_ON()
static bool g_thread_created = false;

bool IsBeforeThreadCreated() {
  return !g_thread_created;
}

void WillCreateThread() {
  g_thread_created = true;
}
#endif

}  // namespace WTF
