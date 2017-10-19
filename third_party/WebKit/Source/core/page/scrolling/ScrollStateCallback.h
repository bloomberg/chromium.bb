// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScrollStateCallback_h
#define ScrollStateCallback_h

#include "platform/heap/Handle.h"
#include "public/platform/WebNativeScrollBehavior.h"

namespace blink {

class ScrollState;

class ScrollStateCallback
    : public GarbageCollectedFinalized<ScrollStateCallback> {
 public:
  ScrollStateCallback()
      : native_scroll_behavior_(WebNativeScrollBehavior::kDisableNativeScroll) {
  }

  virtual ~ScrollStateCallback() {}

  virtual void Trace(blink::Visitor* visitor) {}
  virtual void handleEvent(ScrollState*) = 0;

  void SetNativeScrollBehavior(WebNativeScrollBehavior native_scroll_behavior) {
    DCHECK_LT(static_cast<int>(native_scroll_behavior), 3);
    native_scroll_behavior_ = native_scroll_behavior;
  }

  WebNativeScrollBehavior NativeScrollBehavior() {
    return native_scroll_behavior_;
  }

  static WebNativeScrollBehavior ToNativeScrollBehavior(
      String native_scroll_behavior);

 protected:
  WebNativeScrollBehavior native_scroll_behavior_;
};

}  // namespace blink

#endif  // ScrollStateCallback_h
