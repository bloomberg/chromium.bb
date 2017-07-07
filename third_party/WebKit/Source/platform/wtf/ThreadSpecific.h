/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Jian Li <jianli@chromium.org>
 * Copyright (C) 2012 Patrick Gansterer <paroga@paroga.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Thread local storage is implemented by using either pthread API or Windows
 * native API. There is subtle semantic discrepancy for the cleanup function
 * implementation as noted below:
 *   @ In pthread implementation, the destructor function will be called
 *     repeatedly if there is still non-NULL value associated with the function.
 *   @ In Windows native implementation, the destructor function will be called
 *     only once.
 * This semantic discrepancy does not impose any problem because nowhere in
 * WebKit the repeated call bahavior is utilized.
 */

#ifndef WTF_ThreadSpecific_h
#define WTF_ThreadSpecific_h

#include "build/build_config.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/StackUtil.h"
#include "platform/wtf/StdLibExtras.h"
#include "platform/wtf/WTF.h"
#include "platform/wtf/WTFExport.h"
#include "platform/wtf/allocator/PartitionAllocator.h"
#include "platform/wtf/allocator/Partitions.h"

#if defined(OS_POSIX)
#include <pthread.h>
#elif defined(OS_WIN)
#include <windows.h>
#endif

namespace WTF {

#if defined(OS_WIN)
// ThreadSpecificThreadExit should be called each time when a thread is
// detached.
// This is done automatically for threads created with WTF::createThread.
WTF_EXPORT void ThreadSpecificThreadExit();
#endif

template <typename T>
class ThreadSpecific {
  USING_FAST_MALLOC(ThreadSpecific);
  WTF_MAKE_NONCOPYABLE(ThreadSpecific);

 public:
  ThreadSpecific();
  bool
  IsSet();  // Useful as a fast check to see if this thread has set this value.
  T* operator->();
  operator T*();
  T& operator*();

 private:
#if defined(OS_WIN)
  WTF_EXPORT friend void ThreadSpecificThreadExit();
#endif

  // Not implemented. It's technically possible to destroy a thread specific
  // key, but one would need to make sure that all values have been destroyed
  // already (usually, that all threads that used it have exited). It's
  // unlikely that any user of this call will be in that situation - and having
  // a destructor defined can be confusing, given that it has such strong
  // pre-requisites to work correctly.
  ~ThreadSpecific();

  T* Get();
  void Set(T*);
  void static Destroy(void* ptr);

  struct Data {
    WTF_MAKE_NONCOPYABLE(Data);

   public:
    Data(T* value, ThreadSpecific<T>* owner) : value(value), owner(owner) {}

    T* value;
    ThreadSpecific<T>* owner;
#if defined(OS_WIN)
    void (*destructor)(void*);
#endif
  };

#if defined(OS_POSIX)
  pthread_key_t key_;
#elif defined(OS_WIN)
  int index_;
#endif
  // This member must only be accessed or modified on the main thread.
  T* main_thread_storage_ = nullptr;
};

#if defined(OS_POSIX)

typedef pthread_key_t ThreadSpecificKey;

inline void ThreadSpecificKeyCreate(ThreadSpecificKey* key,
                                    void (*destructor)(void*)) {
  int error = pthread_key_create(key, destructor);
  CHECK(!error);
}

inline void ThreadSpecificKeyDelete(ThreadSpecificKey key) {
  int error = pthread_key_delete(key);
  CHECK(!error);
}

inline void ThreadSpecificSet(ThreadSpecificKey key, void* value) {
  pthread_setspecific(key, value);
}

inline void* ThreadSpecificGet(ThreadSpecificKey key) {
  return pthread_getspecific(key);
}

template <typename T>
inline ThreadSpecific<T>::ThreadSpecific() {
  int error = pthread_key_create(&key_, Destroy);
  CHECK(!error);
}

template <typename T>
inline T* ThreadSpecific<T>::Get() {
  Data* data = static_cast<Data*>(pthread_getspecific(key_));
  return data ? data->value : 0;
}

template <typename T>
inline void ThreadSpecific<T>::Set(T* ptr) {
  DCHECK(!Get());
  pthread_setspecific(key_, new Data(ptr, this));
}

#elif defined(OS_WIN)

// TLS_OUT_OF_INDEXES is not defined on WinCE.
#ifndef TLS_OUT_OF_INDEXES
#define TLS_OUT_OF_INDEXES 0xffffffff
#endif

// The maximum number of TLS keys that can be created. For simplification, we
// assume that:
// 1) Once the instance of ThreadSpecific<> is created, it will not be
//    destructed until the program dies.
// 2) We do not need to hold many instances of ThreadSpecific<> data. This fixed
//    number should be far enough.
const int kMaxTlsKeySize = 256;

WTF_EXPORT long& TlsKeyCount();
WTF_EXPORT DWORD* TlsKeys();

class PlatformThreadSpecificKey;
typedef PlatformThreadSpecificKey* ThreadSpecificKey;

WTF_EXPORT void ThreadSpecificKeyCreate(ThreadSpecificKey*, void (*)(void*));
WTF_EXPORT void ThreadSpecificKeyDelete(ThreadSpecificKey);
WTF_EXPORT void ThreadSpecificSet(ThreadSpecificKey, void*);
WTF_EXPORT void* ThreadSpecificGet(ThreadSpecificKey);

template <typename T>
inline ThreadSpecific<T>::ThreadSpecific() : index_(-1) {
  DWORD tls_key = TlsAlloc();
  CHECK_NE(tls_key, TLS_OUT_OF_INDEXES);

  index_ = InterlockedIncrement(&TlsKeyCount()) - 1;
  CHECK_LE(index_, kMaxTlsKeySize);
  TlsKeys()[index_] = tls_key;
}

template <typename T>
inline ThreadSpecific<T>::~ThreadSpecific() {
  // Does not invoke destructor functions. They will be called from
  // ThreadSpecificThreadExit when the thread is detached.
  TlsFree(tlsKeys()[m_index]);
}

template <typename T>
inline T* ThreadSpecific<T>::Get() {
  Data* data = static_cast<Data*>(TlsGetValue(TlsKeys()[index_]));
  return data ? data->value : 0;
}

template <typename T>
inline void ThreadSpecific<T>::Set(T* ptr) {
  DCHECK(!Get());
  Data* data = new Data(ptr, this);
  data->destructor = &ThreadSpecific<T>::Destroy;
  TlsSetValue(TlsKeys()[index_], data);
}

#else
#error ThreadSpecific is not implemented for this platform.
#endif

template <typename T>
inline void ThreadSpecific<T>::Destroy(void* ptr) {
  Data* data = static_cast<Data*>(ptr);

#if defined(OS_POSIX)
  // We want get() to keep working while data destructor works, because it can
  // be called indirectly by the destructor.  Some pthreads implementations
  // zero out the pointer before calling destroy(), so we temporarily reset it.
  pthread_setspecific(data->owner->key_, ptr);
#endif

  // Never call destructors on the main thread. This is fine because Blink no
  // longer has a graceful shutdown sequence. Be careful to call this function
  // (which can be re-entrant) while the pointer is still set, to avoid lazily
  // allocating WTFThreadData after it is destroyed.
  if (IsMainThread())
    return;

  data->value->~T();
  Partitions::FastFree(data->value);

#if defined(OS_POSIX)
  pthread_setspecific(data->owner->key_, 0);
#elif defined(OS_WIN)
  TlsSetValue(TlsKeys()[data->owner->index_], 0);
#else
#error ThreadSpecific is not implemented for this platform.
#endif

  delete data;
}

template <typename T>
inline bool ThreadSpecific<T>::IsSet() {
  return !!Get();
}

template <typename T>
inline ThreadSpecific<T>::operator T*() {
  T* off_thread_ptr;
#if defined(__GLIBC__) || defined(OS_ANDROID) || defined(OS_FREEBSD)
  // TLS is fast on these platforms.
  // TODO(csharrison): Qualify this statement for Android.
  const bool kMainThreadAlwaysChecksTLS = true;
  T** ptr = &off_thread_ptr;
  off_thread_ptr = static_cast<T*>(Get());
#else
  const bool kMainThreadAlwaysChecksTLS = false;
  T** ptr = &main_thread_storage_;
  if (UNLIKELY(MayNotBeMainThread())) {
    off_thread_ptr = static_cast<T*>(Get());
    ptr = &off_thread_ptr;
  }
#endif
  // Set up thread-specific value's memory pointer before invoking constructor,
  // in case any function it calls needs to access the value, to avoid
  // recursion.
  if (UNLIKELY(!*ptr)) {
    *ptr = static_cast<T*>(Partitions::FastZeroedMalloc(
        sizeof(T), WTF_HEAP_PROFILER_TYPE_NAME(T)));

    // Even if we didn't realize we're on the main thread, we might still be.
    // We need to double-check so that |m_mainThreadStorage| is populated.
    if (!kMainThreadAlwaysChecksTLS && UNLIKELY(ptr != &main_thread_storage_) &&
        IsMainThread()) {
      main_thread_storage_ = *ptr;
    }

    Set(*ptr);
    new (NotNull, *ptr) T;
  }
  return *ptr;
}

template <typename T>
inline T* ThreadSpecific<T>::operator->() {
  return operator T*();
}

template <typename T>
inline T& ThreadSpecific<T>::operator*() {
  return *operator T*();
}

}  // namespace WTF

using WTF::ThreadSpecific;

#endif  // WTF_ThreadSpecific_h
