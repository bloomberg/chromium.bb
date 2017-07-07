/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2009 Torch Mobile, Inc. All rights reserved.
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

/*
 * There are numerous academic and practical works on how to implement
 * pthread_cond_wait/pthread_cond_signal/pthread_cond_broadcast
 * functions on Win32. Here is one example:
 * http://www.cs.wustl.edu/~schmidt/win32-cv-1.html which is widely credited as
 * a 'starting point' of modern attempts. There are several more or less proven
 * implementations, one in Boost C++ library (http://www.boost.org) and another
 * in pthreads-win32 (http://sourceware.org/pthreads-win32/).
 *
 * The number of articles and discussions is the evidence of significant
 * difficulties in implementing these primitives correctly.  The brief search
 * of revisions, ChangeLog entries, discussions in comp.programming.threads and
 * other places clearly documents numerous pitfalls and performance problems
 * the authors had to overcome to arrive to the suitable implementations.
 * Optimally, WebKit would use one of those supported/tested libraries
 * directly.  To roll out our own implementation is impractical, if even for
 * the lack of sufficient testing. However, a faithful reproduction of the code
 * from one of the popular supported libraries seems to be a good compromise.
 *
 * The early Boost implementation
 * (http://www.boxbackup.org/trac/browser/box/nick/win/lib/win32/boost_1_32_0/libs/thread/src/condition.cpp?rev=30)
 * is identical to pthreads-win32
 * (http://sourceware.org/cgi-bin/cvsweb.cgi/pthreads/pthread_cond_wait.c?rev=1.10&content-type=text/x-cvsweb-markup&cvsroot=pthreads-win32).
 * Current Boost uses yet another (although seemingly equivalent) algorithm
 * which came from their 'thread rewrite' effort.
 *
 * This file includes timedWait/signal/broadcast implementations translated to
 * WebKit coding style from the latest algorithm by Alexander Terekhov and
 * Louis Thomas, as captured here:
 * http://sourceware.org/cgi-bin/cvsweb.cgi/pthreads/pthread_cond_wait.c?rev=1.10&content-type=text/x-cvsweb-markup&cvsroot=pthreads-win32
 * It replaces the implementation of their previous algorithm, also documented
 * in the same source above.  The naming and comments are left very close to
 * original to enable easy cross-check.
 *
 * The corresponding Pthreads-win32 License is included below, and CONTRIBUTORS
 * file which it refers to is added to source directory (as
 * CONTRIBUTORS.pthreads-win32).
 */

/*
 *      Pthreads-win32 - POSIX Threads Library for Win32
 *      Copyright(C) 1998 John E. Bossom
 *      Copyright(C) 1999,2005 Pthreads-win32 contributors
 *
 *      Contact Email: rpj@callisto.canberra.edu.au
 *
 *      The current list of contributors is contained
 *      in the file CONTRIBUTORS included with the source
 *      code distribution. The list can also be seen at the
 *      following World Wide Web location:
 *      http://sources.redhat.com/pthreads-win32/contributors.html
 *
 *      This library is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU Lesser General Public
 *      License as published by the Free Software Foundation; either
 *      version 2 of the License, or (at your option) any later version.
 *
 *      This library is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *      Lesser General Public License for more details.
 *
 *      You should have received a copy of the GNU Lesser General Public
 *      License along with this library in the file COPYING.LIB;
 *      if not, write to the Free Software Foundation, Inc.,
 *      59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#include "platform/wtf/Threading.h"

#include "build/build_config.h"

#if defined(OS_WIN)

#include "platform/wtf/CurrentTime.h"
#include "platform/wtf/DateMath.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/MathExtras.h"
#include "platform/wtf/ThreadSpecific.h"
#include "platform/wtf/ThreadingPrimitives.h"
#include "platform/wtf/WTFThreadData.h"
#include "platform/wtf/dtoa/double-conversion.h"
#include <errno.h>
#include <process.h>
#include <windows.h>

namespace WTF {

// THREADNAME_INFO comes from
// <http://msdn.microsoft.com/en-us/library/xcb2z8hs.aspx>.
#pragma pack(push, 8)
typedef struct tagTHREADNAME_INFO {
  DWORD dw_type;       // must be 0x1000
  LPCSTR sz_name;      // pointer to name (in user addr space)
  DWORD dw_thread_id;  // thread ID (-1=caller thread)
  DWORD dw_flags;      // reserved for future use, must be zero
} THREADNAME_INFO;
#pragma pack(pop)

namespace internal {

ThreadIdentifier CurrentThreadSyscall() {
  return static_cast<ThreadIdentifier>(GetCurrentThreadId());
}

}  // namespace internal

void InitializeThreading() {
  // This should only be called once.
  WTFThreadData::Initialize();

  InitializeDates();
  // Force initialization of static DoubleToStringConverter converter variable
  // inside EcmaScriptConverter function while we are in single thread mode.
  double_conversion::DoubleToStringConverter::EcmaScriptConverter();
}

namespace {
ThreadSpecificKey g_current_thread_key;
bool g_current_thread_key_initialized = false;
}  // namespace

void InitializeCurrentThread() {
  DCHECK(!g_current_thread_key_initialized);
  ThreadSpecificKeyCreate(&g_current_thread_key, [](void*) {});
  g_current_thread_key_initialized = true;
}

ThreadIdentifier CurrentThread() {
  // This doesn't use WTF::ThreadSpecific (e.g. WTFThreadData) because
  // ThreadSpecific now depends on currentThread. It is necessary to avoid this
  // or a similar loop:
  //
  // currentThread
  // -> wtfThreadData
  // -> ThreadSpecific::operator*
  // -> isMainThread
  // -> currentThread
  static_assert(sizeof(ThreadIdentifier) <= sizeof(void*),
                "ThreadIdentifier must fit in a void*.");
  DCHECK(g_current_thread_key_initialized);
  void* value = ThreadSpecificGet(g_current_thread_key);
  if (UNLIKELY(!value)) {
    value = reinterpret_cast<void*>(internal::CurrentThreadSyscall());
    DCHECK(value);
    ThreadSpecificSet(g_current_thread_key, value);
  }
  return reinterpret_cast<intptr_t>(ThreadSpecificGet(g_current_thread_key));
}

MutexBase::MutexBase(bool recursive) {
  mutex_.recursion_count_ = 0;
  InitializeCriticalSection(&mutex_.internal_mutex_);
}

MutexBase::~MutexBase() {
  DeleteCriticalSection(&mutex_.internal_mutex_);
}

void MutexBase::lock() {
  EnterCriticalSection(&mutex_.internal_mutex_);
  ++mutex_.recursion_count_;
}

void MutexBase::unlock() {
  DCHECK(mutex_.recursion_count_);
  --mutex_.recursion_count_;
  LeaveCriticalSection(&mutex_.internal_mutex_);
}

bool Mutex::TryLock() {
  // This method is modeled after the behavior of pthread_mutex_trylock,
  // which will return an error if the lock is already owned by the
  // current thread.  Since the primitive Win32 'TryEnterCriticalSection'
  // treats this as a successful case, it changes the behavior of several
  // tests in WebKit that check to see if the current thread already
  // owned this mutex (see e.g., IconDatabase::getOrCreateIconRecord)
  DWORD result = TryEnterCriticalSection(&mutex_.internal_mutex_);

  if (result != 0) {  // We got the lock
    // If this thread already had the lock, we must unlock and return
    // false since this is a non-recursive mutex. This is to mimic the
    // behavior of POSIX's pthread_mutex_trylock. We don't do this
    // check in the lock method (presumably due to performance?). This
    // means lock() will succeed even if the current thread has already
    // entered the critical section.
    if (mutex_.recursion_count_ > 0) {
      LeaveCriticalSection(&mutex_.internal_mutex_);
      return false;
    }
    ++mutex_.recursion_count_;
    return true;
  }

  return false;
}

bool RecursiveMutex::TryLock() {
  // CRITICAL_SECTION is recursive/reentrant so TryEnterCriticalSection will
  // succeed if the current thread is already in the critical section.
  DWORD result = TryEnterCriticalSection(&mutex_.internal_mutex_);
  if (result == 0) {  // We didn't get the lock.
    return false;
  }
  ++mutex_.recursion_count_;
  return true;
}

bool PlatformCondition::TimedWait(PlatformMutex& mutex,
                                  DWORD duration_milliseconds) {
  // Enter the wait state.
  DWORD res = WaitForSingleObject(block_lock_, INFINITE);
  DCHECK_EQ(res, WAIT_OBJECT_0);
  ++waiters_blocked_;
  res = ReleaseSemaphore(block_lock_, 1, 0);
  DCHECK(res);

  --mutex.recursion_count_;
  LeaveCriticalSection(&mutex.internal_mutex_);

  // Main wait - use timeout.
  bool timed_out = (WaitForSingleObject(block_queue_, duration_milliseconds) ==
                    WAIT_TIMEOUT);

  res = WaitForSingleObject(unblock_lock_, INFINITE);
  DCHECK_EQ(res, WAIT_OBJECT_0);

  int signals_left = waiters_to_unblock_;

  if (waiters_to_unblock_) {
    --waiters_to_unblock_;
  } else if (++waiters_gone_ == (INT_MAX / 2)) {
    // timeout/canceled or spurious semaphore timeout or spurious wakeup
    // occured, normalize the m_waitersGone count this may occur if many
    // calls to wait with a timeout are made and no call to notify_* is made
    res = WaitForSingleObject(block_lock_, INFINITE);
    DCHECK_EQ(res, WAIT_OBJECT_0);
    waiters_blocked_ -= waiters_gone_;
    res = ReleaseSemaphore(block_lock_, 1, 0);
    DCHECK(res);
    waiters_gone_ = 0;
  }

  res = ReleaseMutex(unblock_lock_);
  DCHECK(res);

  if (signals_left == 1) {
    res = ReleaseSemaphore(block_lock_, 1, 0);  // Open the gate.
    DCHECK(res);
  }

  EnterCriticalSection(&mutex.internal_mutex_);
  ++mutex.recursion_count_;

  return !timed_out;
}

void PlatformCondition::Signal(bool unblock_all) {
  unsigned signals_to_issue = 0;

  DWORD res = WaitForSingleObject(unblock_lock_, INFINITE);
  DCHECK_EQ(res, WAIT_OBJECT_0);

  if (waiters_to_unblock_) {  // the gate is already closed
    if (!waiters_blocked_) {  // no-op
      res = ReleaseMutex(unblock_lock_);
      DCHECK(res);
      return;
    }

    if (unblock_all) {
      signals_to_issue = waiters_blocked_;
      waiters_to_unblock_ += waiters_blocked_;
      waiters_blocked_ = 0;
    } else {
      signals_to_issue = 1;
      ++waiters_to_unblock_;
      --waiters_blocked_;
    }
  } else if (waiters_blocked_ > waiters_gone_) {
    res = WaitForSingleObject(block_lock_, INFINITE);  // Close the gate.
    DCHECK_EQ(res, WAIT_OBJECT_0);
    if (waiters_gone_ != 0) {
      waiters_blocked_ -= waiters_gone_;
      waiters_gone_ = 0;
    }
    if (unblock_all) {
      signals_to_issue = waiters_blocked_;
      waiters_to_unblock_ = waiters_blocked_;
      waiters_blocked_ = 0;
    } else {
      signals_to_issue = 1;
      waiters_to_unblock_ = 1;
      --waiters_blocked_;
    }
  } else {  // No-op.
    res = ReleaseMutex(unblock_lock_);
    DCHECK(res);
    return;
  }

  res = ReleaseMutex(unblock_lock_);
  DCHECK(res);

  if (signals_to_issue) {
    res = ReleaseSemaphore(block_queue_, signals_to_issue, 0);
    DCHECK(res);
  }
}

static const long kMaxSemaphoreCount = static_cast<long>(~0UL >> 1);

ThreadCondition::ThreadCondition() {
  condition_.waiters_gone_ = 0;
  condition_.waiters_blocked_ = 0;
  condition_.waiters_to_unblock_ = 0;
  condition_.block_lock_ = CreateSemaphore(0, 1, 1, 0);
  condition_.block_queue_ = CreateSemaphore(0, 0, kMaxSemaphoreCount, 0);
  condition_.unblock_lock_ = CreateMutex(0, 0, 0);

  if (!condition_.block_lock_ || !condition_.block_queue_ ||
      !condition_.unblock_lock_) {
    if (condition_.block_lock_)
      CloseHandle(condition_.block_lock_);
    if (condition_.block_queue_)
      CloseHandle(condition_.block_queue_);
    if (condition_.unblock_lock_)
      CloseHandle(condition_.unblock_lock_);

    condition_.block_lock_ = nullptr;
    condition_.block_queue_ = nullptr;
    condition_.unblock_lock_ = nullptr;
  }
}

ThreadCondition::~ThreadCondition() {
  if (condition_.block_lock_)
    CloseHandle(condition_.block_lock_);
  if (condition_.block_queue_)
    CloseHandle(condition_.block_queue_);
  if (condition_.unblock_lock_)
    CloseHandle(condition_.unblock_lock_);
}

void ThreadCondition::Wait(MutexBase& mutex) {
  condition_.TimedWait(mutex.Impl(), INFINITE);
}

bool ThreadCondition::TimedWait(MutexBase& mutex, double absolute_time) {
  DWORD interval = AbsoluteTimeToWaitTimeoutInterval(absolute_time);

  if (!interval) {
    // Consider the wait to have timed out, even if our condition has already
    // been signaled, to match the pthreads implementation.
    return false;
  }

  return condition_.TimedWait(mutex.Impl(), interval);
}

void ThreadCondition::Signal() {
  condition_.Signal(false);  // Unblock only 1 thread.
}

void ThreadCondition::Broadcast() {
  condition_.Signal(true);  // Unblock all threads.
}

DWORD AbsoluteTimeToWaitTimeoutInterval(double absolute_time) {
  double current_time = WTF::CurrentTime();

  // Time is in the past - return immediately.
  if (absolute_time < current_time)
    return 0;

  // Time is too far in the future (and would overflow unsigned long) - wait
  // forever.
  if (absolute_time - current_time > static_cast<double>(INT_MAX) / 1000.0)
    return INFINITE;

  return static_cast<DWORD>((absolute_time - current_time) * 1000.0);
}

#if DCHECK_IS_ON()
static bool g_thread_created = false;

Mutex& GetThreadCreatedMutex() {
  static Mutex g_thread_created_mutex;
  return g_thread_created_mutex;
}

bool IsBeforeThreadCreated() {
  MutexLocker locker(GetThreadCreatedMutex());
  return !g_thread_created;
}

void WillCreateThread() {
  MutexLocker locker(GetThreadCreatedMutex());
  g_thread_created = true;
}
#endif

}  // namespace WTF

#endif  // defined(OS_WIN)
