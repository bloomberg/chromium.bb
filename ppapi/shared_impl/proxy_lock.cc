// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/proxy_lock.h"

#include "base/synchronization/lock.h"

namespace ppapi {

base::Lock* ProxyLock::lock_ = NULL;

// static
void ProxyLock::Acquire() {
  if (lock_)
    lock_->Acquire();
}

// static
void ProxyLock::Release() {
  if (lock_)
    lock_->Release();
}

// static
void ProxyLock::Set(base::Lock* lock) {
  lock_ = lock;
}

// static
void ProxyLock::Reset() {
  Set(NULL);
}

}  // namespace ppapi
