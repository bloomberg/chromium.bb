// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PROXY_LOCK_H_
#define PPAPI_SHARED_IMPL_PROXY_LOCK_H_

#include "base/basictypes.h"

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


}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PROXY_LOCK_H_
