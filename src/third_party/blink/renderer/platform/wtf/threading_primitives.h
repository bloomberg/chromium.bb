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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_THREADING_PRIMITIVES_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_THREADING_PRIMITIVES_H_

#include "base/dcheck_is_on.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/wtf_export.h"

#if BUILDFLAG(IS_WIN)

#include "base/win/windows_types.h"

// Declare Chrome versions of some Windows structures. These are needed for
// when we need a concrete type but don't want to pull in Windows.h. We can't
// declare the Windows types so we declare our types and cast to the Windows
// types in a few places. static_asserts in threading_win.cc are used to verify
// that the sizes are correct.

struct BLINK_CRITICAL_SECTION {
  // The Windows CRITICAL_SECTION struct is 40 bytes on 64-bit and 24 bytes on
  // 32-bit. The align member variable uses sizeof(void*) bytes so the buffer
  // to fill out the size needs to be 32/20 bytes. This can be expressed as
  // sizeof(void*) * 3 + 8.
  char buffer[sizeof(void*) * 3 + 8];
  ULONG_PTR align;  // Make sure the alignment requirements match.
};

#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#include <pthread.h>
#endif

namespace blink {
class DeferredTaskHandler;
}

namespace WTF {

#if BUILDFLAG(IS_WIN)
struct PlatformMutex {
  BLINK_CRITICAL_SECTION internal_mutex_;
  size_t recursion_count_;
};
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
struct PlatformMutex {
  pthread_mutex_t internal_mutex_;
#if DCHECK_IS_ON()
  size_t recursion_count_;
#endif
};
#endif

class WTF_EXPORT MutexBase {
  USING_FAST_MALLOC(MutexBase);

 public:
  MutexBase(const MutexBase&) = delete;
  MutexBase& operator=(const MutexBase&) = delete;
  ~MutexBase();

  void lock();
  void unlock();
  void AssertAcquired() const {
#if DCHECK_IS_ON()
    DCHECK(mutex_.recursion_count_);
#endif
  }

 public:
  PlatformMutex& Impl() { return mutex_; }

 protected:
  MutexBase(bool recursive);

  PlatformMutex mutex_;
};

class ThreadCondition;

// Note: Prefer base::Lock to WTF::Mutex. The implementation is the same, this
// will be removed. crbug.com/1290281.
class LOCKABLE WTF_EXPORT Mutex {
 public:
  Mutex() = default;
  bool TryLock() EXCLUSIVE_TRYLOCK_FUNCTION(true) { return lock_.Try(); }

  // Overridden solely for the purpose of annotating them.
  // The compiler is expected to optimize the calls away.
  void lock() EXCLUSIVE_LOCK_FUNCTION() { lock_.Acquire(); }
  void unlock() UNLOCK_FUNCTION() { lock_.Release(); }
  void AssertAcquired() const ASSERT_EXCLUSIVE_LOCK() {
    lock_.AssertAcquired();
  }

 private:
  base::Lock lock_;

  friend class ThreadCondition;
};

// RecursiveMutex is deprecated AND WILL BE REMOVED.
// https://crbug.com/856641
class WTF_EXPORT RecursiveMutex : public MutexBase {
 public:
  bool TryLock();

 private:
  // Private constructor to ensure that no new users appear. This class will be
  // removed.
  RecursiveMutex() : MutexBase(true) {}

  // DO NOT ADD any new caller.
  friend class ::blink::DeferredTaskHandler;
};

class SCOPED_LOCKABLE MutexLocker final {
  STACK_ALLOCATED();

 public:
  MutexLocker(Mutex& mutex) EXCLUSIVE_LOCK_FUNCTION(mutex) : mutex_(mutex) {
    mutex_.lock();
  }
  MutexLocker(const MutexLocker&) = delete;
  MutexLocker& operator=(const MutexLocker&) = delete;
  ~MutexLocker() UNLOCK_FUNCTION() { mutex_.unlock(); }

 private:
  Mutex& mutex_;
};

class MutexTryLocker final {
  STACK_ALLOCATED();

 public:
  MutexTryLocker(Mutex& mutex) : mutex_(mutex), locked_(mutex.TryLock()) {}
  MutexTryLocker(const MutexTryLocker&) = delete;
  MutexTryLocker& operator=(const MutexTryLocker&) = delete;
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

 public:
  explicit ThreadCondition(Mutex& mutex) : cv_(&mutex.lock_) {}
  ThreadCondition(const ThreadCondition&) = delete;
  ThreadCondition& operator=(const ThreadCondition&) = delete;
  ~ThreadCondition() = default;

  void Wait() { cv_.Wait(); }
  void Signal() { cv_.Signal(); }
  void Broadcast() { cv_.Broadcast(); }

 private:
  base::ConditionVariable cv_;
};

}  // namespace WTF

using WTF::Mutex;
using WTF::MutexBase;
using WTF::MutexLocker;
using WTF::MutexTryLocker;
using WTF::RecursiveMutex;
using WTF::ThreadCondition;

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_THREADING_PRIMITIVES_H_
