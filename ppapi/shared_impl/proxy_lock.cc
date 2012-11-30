// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/proxy_lock.h"

#include "base/lazy_instance.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_local.h"
#include "ppapi/shared_impl/ppapi_globals.h"

namespace ppapi {

// Simple single-thread deadlock detector for the proxy lock.
// |true| when the current thread has the lock.
base::LazyInstance<base::ThreadLocalBoolean>::Leaky
    g_proxy_locked_on_thread = LAZY_INSTANCE_INITIALIZER;

// static
void ProxyLock::Acquire() {
  base::Lock* lock(PpapiGlobals::Get()->GetProxyLock());
  if (lock) {
    // This thread does not already hold the lock.
    const bool deadlock = g_proxy_locked_on_thread.Get().Get();
    CHECK(!deadlock);

    lock->Acquire();
    g_proxy_locked_on_thread.Get().Set(true);
  }
}

// static
void ProxyLock::Release() {
  base::Lock* lock(PpapiGlobals::Get()->GetProxyLock());
  if (lock) {
    // This thread currently holds the lock.
    const bool locked = g_proxy_locked_on_thread.Get().Get();
    CHECK(locked);

    g_proxy_locked_on_thread.Get().Set(false);
    lock->Release();
  }
}

// static
void ProxyLock::AssertAcquired() {
  base::Lock* lock(PpapiGlobals::Get()->GetProxyLock());
  if (lock) {
    // This thread currently holds the lock.
    const bool locked = g_proxy_locked_on_thread.Get().Get();
    CHECK(locked);

    lock->AssertAcquired();
  }
}

void CallWhileUnlocked(const base::Closure& closure) {
  ProxyAutoUnlock lock;
  closure.Run();
}

void CallWhileLocked(const base::Closure& closure) {
  ProxyAutoLock lock;
  closure.Run();
}

}  // namespace ppapi
