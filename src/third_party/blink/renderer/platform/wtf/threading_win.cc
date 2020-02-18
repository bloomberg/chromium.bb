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

#include "third_party/blink/renderer/platform/wtf/threading.h"

#include "build/build_config.h"

#if defined(OS_WIN)

#include <errno.h>
#include <process.h>
#include <windows.h>
#include "base/threading/scoped_blocking_call.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"

namespace WTF {

MutexBase::MutexBase(bool recursive) {
  mutex_.recursion_count_ = 0;
  InitializeCriticalSection(&mutex_.internal_mutex_);
}

MutexBase::~MutexBase() {
  DeleteCriticalSection(&mutex_.internal_mutex_);
}

void MutexBase::lock() {
  EnterCriticalSection(&mutex_.internal_mutex_);
  DCHECK(!mutex_.recursion_count_)
      << "WTF does not support recursive mutex acquisition!";
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
    DCHECK(!mutex_.recursion_count_)
        << "WTF does not support recursive mutex acquisition!";
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
  DCHECK(!mutex_.recursion_count_)
      << "WTF does not support recursive mutex acquisition!";
  ++mutex_.recursion_count_;
  return true;
}

ThreadCondition::ThreadCondition(Mutex& mutex)
  : condition_(CONDITION_VARIABLE_INIT), mutex_(mutex.Impl()) {}

ThreadCondition::~ThreadCondition() {}

void ThreadCondition::Wait() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  --mutex_.recursion_count_;
  BOOL result =
      SleepConditionVariableCS(&condition_, &mutex_.internal_mutex_, INFINITE);
  DCHECK_NE(result, 0);
  ++mutex_.recursion_count_;
}

void ThreadCondition::Signal() {
  WakeConditionVariable(&condition_);
}

void ThreadCondition::Broadcast() {
  WakeAllConditionVariable(&condition_);
}

}  // namespace WTF

#endif  // defined(OS_WIN)
