// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PROXY_LOCK_H_
#define PPAPI_SHARED_IMPL_PROXY_LOCK_H_

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"

#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace base {
class Lock;
}

namespace ppapi {

// This is the one lock to rule them all for the ppapi proxy. All PPB interface
// functions that need to be synchronized should lock this lock on entry. This
// is normally accomplished by using an appropriate Enter RAII object at the
// beginning of each thunk function.
//
// TODO(dmichael): If this turns out to be too slow and contentious, we'll want
// to use multiple locks. E.g., one for the var tracker, one for the resource
// tracker, etc.
class PPAPI_SHARED_EXPORT ProxyLock {
 public:
  // Acquire the proxy lock. If it is currently held by another thread, block
  // until it is available. If the lock has not been set using the 'Set' method,
  // this operation does nothing. That is the normal case for the host side;
  // see PluginResourceTracker for where the lock gets set for the out-of-
  // process plugin case.
  static void Acquire();
  // Relinquish the proxy lock. If the lock has not been set, this does nothing.
  static void Release();

  // Assert that the lock is owned by the current thread (in the plugin
  // process). Does nothing when running in-process (or in the host process).
  static void AssertAcquired();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ProxyLock);
};

// A simple RAII class for locking the PPAPI proxy lock on entry and releasing
// on exit. This is for simple interfaces that don't use the 'thunk' system,
// such as PPB_Var and PPB_Core.
class ProxyAutoLock {
 public:
  ProxyAutoLock() {
    ProxyLock::Acquire();
  }
  ~ProxyAutoLock() {
    ProxyLock::Release();
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(ProxyAutoLock);
};

// The inverse of the above; unlock on construction, lock on destruction. This
// is useful for calling out to the plugin, when we need to unlock but ensure
// that we re-acquire the lock when the plugin is returns or raises an
// exception.
class ProxyAutoUnlock {
 public:
  ProxyAutoUnlock() {
    ProxyLock::Release();
  }
  ~ProxyAutoUnlock() {
    ProxyLock::Acquire();
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(ProxyAutoUnlock);
};

// A set of function template overloads for invoking a function pointer while
// the ProxyLock is unlocked. This assumes that the luck is held.
// CallWhileUnlocked unlocks the ProxyLock just before invoking the given
// function. The lock is immediately re-acquired when the invoked function
// function returns. CallWhileUnlocked returns whatever the given function
// returned.
//
// Example usage:
//   *result = CallWhileUnlocked(ppp_input_event_impl_->HandleInputEvent,
//                               instance,
//                               resource->pp_resource());
template <class ReturnType>
ReturnType CallWhileUnlocked(ReturnType (*function)()) {
  ProxyAutoUnlock unlock;
  return function();
}
template <class ReturnType, class P1>
ReturnType CallWhileUnlocked(ReturnType (*function)(P1), const P1& p1) {
  ProxyAutoUnlock unlock;
  return function(p1);
}
template <class ReturnType, class P1, class P2>
ReturnType CallWhileUnlocked(ReturnType (*function)(P1, P2),
                             const P1& p1,
                             const P2& p2) {
  ProxyAutoUnlock unlock;
  return function(p1, p2);
}
template <class ReturnType, class P1, class P2, class P3>
ReturnType CallWhileUnlocked(ReturnType (*function)(P1, P2, P3),
                             const P1& p1,
                             const P2& p2,
                             const P3& p3) {
  ProxyAutoUnlock unlock;
  return function(p1, p2, p3);
}
template <class ReturnType, class P1, class P2, class P3, class P4>
ReturnType CallWhileUnlocked(ReturnType (*function)(P1, P2, P3, P4),
                             const P1& p1,
                             const P2& p2,
                             const P3& p3,
                             const P4& p4) {
  ProxyAutoUnlock unlock;
  return function(p1, p2, p3, p4);
}
template <class ReturnType, class P1, class P2, class P3, class P4, class P5>
ReturnType CallWhileUnlocked(ReturnType (*function)(P1, P2, P3, P4, P5),
                             const P1& p1,
                             const P2& p2,
                             const P3& p3,
                             const P4& p4,
                             const P5& p5) {
  ProxyAutoUnlock unlock;
  return function(p1, p2, p3, p4, p5);
}
void PPAPI_SHARED_EXPORT CallWhileUnlocked(const base::Closure& closure);

// CallWhileLocked locks the ProxyLock and runs the given closure immediately.
// The lock is released when CallWhileLocked returns. This function assumes the
// lock is not held. This is mostly for use in RunWhileLocked; see below.
void PPAPI_SHARED_EXPORT CallWhileLocked(const base::Closure& closure);

// RunWhileLocked binds the given closure with CallWhileLocked and returns the
// new Closure. This is for cases where you want to run a task, but you want to
// ensure that the ProxyLock is acquired for the duration of the task.
// Example usage:
//   GetMainThreadMessageLoop()->PostDelayedTask(
//     FROM_HERE,
//     RunWhileLocked(base::Bind(&CallbackWrapper, callback, result)),
//     delay_in_ms);
inline base::Closure RunWhileLocked(const base::Closure& closure) {
  return base::Bind(CallWhileLocked, closure);
}

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PROXY_LOCK_H_
