// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/wake_lock/wake_lock_request.h"

namespace blink {

WakeLockRequest::WakeLockRequest(WakeLock* wake_lock)
    : owner_wake_lock_(wake_lock) {}

WakeLockRequest::~WakeLockRequest() = default;

void WakeLockRequest::cancel() {
  if (cancelled_)
    return;

  cancelled_ = true;
  owner_wake_lock_->CancelRequest();
}

void WakeLockRequest::Trace(blink::Visitor* visitor) {
  visitor->Trace(owner_wake_lock_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
