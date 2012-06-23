// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/proxy_lock.h"

#include "base/synchronization/lock.h"
#include "ppapi/shared_impl/ppapi_globals.h"

namespace ppapi {

// static
void ProxyLock::Acquire() {
  base::Lock* lock(PpapiGlobals::Get()->GetProxyLock());
  if (lock)
    lock->Acquire();
}

// static
void ProxyLock::Release() {
  base::Lock* lock(PpapiGlobals::Get()->GetProxyLock());
  if (lock)
    lock->Release();
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
