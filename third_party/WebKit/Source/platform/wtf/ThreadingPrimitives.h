/*
 * Copyright (C) 2007, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Justin Haygood (jhaygood@reaktix.com)
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
 *
 */

#ifndef ThreadingPrimitives_h
#define ThreadingPrimitives_h

#include "build/build_config.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/Locker.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/WTFExport.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#if defined(OS_POSIX)
#include <pthread.h>
#endif

namespace WTF {

#if defined(OS_POSIX)
struct PlatformMutex {
  pthread_mutex_t internal_mutex_;
#if DCHECK_IS_ON()
  size_t recursion_count_;
#endif
};
typedef pthread_cond_t PlatformCondition;
#elif defined(OS_WIN)
struct PlatformMutex {
  CRITICAL_SECTION internal_mutex_;
  size_t recursion_count_;
};
struct PlatformCondition {
  size_t waiters_gone_;
  size_t waiters_blocked_;
  size_t waiters_to_unblock_;
  HANDLE block_lock_;
  HANDLE block_queue_;
  HANDLE unblock_lock_;

  bool TimedWait(PlatformMutex&, DWORD duration_milliseconds);
  void Signal(bool unblock_all);
};
#else
typedef void* PlatformMutex;
typedef void* PlatformCondition;
#endif

class WTF_EXPORT MutexBase {
  WTF_MAKE_NONCOPYABLE(MutexBase);
  USING_FAST_MALLOC(MutexBase);

 public:
  ~MutexBase();

  void lock();
  void unlock();
#if DCHECK_IS_ON()
  bool Locked() { return mutex_.recursion_count_ > 0; }
#endif

 public:
  PlatformMutex& Impl() { return mutex_; }

 protected:
  MutexBase(bool recursive);

  PlatformMutex mutex_;
};

class WTF_EXPORT Mutex : public MutexBase {
 public:
  Mutex() : MutexBase(false) {}
  bool TryLock();
};

class WTF_EXPORT RecursiveMutex : public MutexBase {
 public:
  RecursiveMutex() : MutexBase(true) {}
  bool TryLock();
};

typedef Locker<MutexBase> MutexLocker;

class MutexTryLocker final {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(MutexTryLocker);

 public:
  MutexTryLocker(Mutex& mutex) : mutex_(mutex), locked_(mutex.TryLock()) {}
  ~MutexTryLocker() {
    if (locked_)
      mutex_.unlock();
  }

  bool Locked() const { return locked_; }

 private:
  Mutex& mutex_;
  bool locked_;
};

class WTF_EXPORT ThreadCondition final {
  USING_FAST_MALLOC(ThreadCondition);  // Only HeapTest.cpp requires.
  WTF_MAKE_NONCOPYABLE(ThreadCondition);

 public:
  ThreadCondition();
  ~ThreadCondition();

  void Wait(MutexBase&);
  // Returns true if the condition was signaled before absoluteTime, false if
  // the absoluteTime was reached or is in the past.
  // The absoluteTime is in seconds, starting on January 1, 1970. The time is
  // assumed to use the same time zone as WTF::currentTime().
  bool TimedWait(MutexBase&, double absolute_time);
  void Signal();
  void Broadcast();

 private:
  PlatformCondition condition_;
};

#if defined(OS_WIN)
// The absoluteTime is in seconds, starting on January 1, 1970. The time is
// assumed to use the same time zone as WTF::currentTime().
// Returns an interval in milliseconds suitable for passing to one of the Win32
// wait functions (e.g., ::WaitForSingleObject).
DWORD AbsoluteTimeToWaitTimeoutInterval(double absolute_time);
#endif

}  // namespace WTF

using WTF::MutexBase;
using WTF::Mutex;
using WTF::RecursiveMutex;
using WTF::MutexLocker;
using WTF::MutexTryLocker;
using WTF::ThreadCondition;

#if defined(OS_WIN)
using WTF::AbsoluteTimeToWaitTimeoutInterval;
#endif

#endif  // ThreadingPrimitives_h
