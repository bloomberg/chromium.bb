// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/wake_lock/navigator_wake_lock.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/wake_lock/wake_lock.h"

namespace blink {

NavigatorWakeLock::NavigatorWakeLock(Navigator& navigator)
    : Supplement<Navigator>(navigator) {}

WakeLock* NavigatorWakeLock::GetWakeLock() {
  if (!wake_lock_) {
    if (auto* window = GetSupplementable()->DomWindow())
      wake_lock_ = MakeGarbageCollected<WakeLock>(*window);
  }
  return wake_lock_;
}

// static
const char NavigatorWakeLock::kSupplementName[] = "NavigatorWakeLock";

// static
NavigatorWakeLock& NavigatorWakeLock::From(Navigator& navigator) {
  NavigatorWakeLock* supplement =
      Supplement<Navigator>::From<NavigatorWakeLock>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorWakeLock>(navigator);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

// static
WakeLock* NavigatorWakeLock::wakeLock(Navigator& navigator) {
  return NavigatorWakeLock::From(navigator).GetWakeLock();
}

void NavigatorWakeLock::Trace(Visitor* visitor) {
  visitor->Trace(wake_lock_);
  Supplement<Navigator>::Trace(visitor);
}

}  // namespace blink
