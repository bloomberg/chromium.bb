// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/scrolling/ScrollStateCallback.h"

#include "platform/wtf/text/WTFString.h"

namespace blink {

void ScrollStateCallbackV8Impl::Trace(blink::Visitor* visitor) {
  visitor->Trace(callback_);
  ScrollStateCallback::Trace(visitor);
}

void ScrollStateCallbackV8Impl::Invoke(ScrollState* scroll_state) {
  callback_->InvokeAndReportException(nullptr, scroll_state);
}

WebNativeScrollBehavior ScrollStateCallbackV8Impl::ParseNativeScrollBehavior(
    const String& native_scroll_behavior) {
  static const char kDisable[] = "disable-native-scroll";
  static const char kBefore[] = "perform-before-native-scroll";
  static const char kAfter[] = "perform-after-native-scroll";

  if (native_scroll_behavior == kDisable)
    return WebNativeScrollBehavior::kDisableNativeScroll;
  if (native_scroll_behavior == kBefore)
    return WebNativeScrollBehavior::kPerformBeforeNativeScroll;
  if (native_scroll_behavior == kAfter)
    return WebNativeScrollBehavior::kPerformAfterNativeScroll;

  NOTREACHED();
  return WebNativeScrollBehavior::kDisableNativeScroll;
}

}  // namespace blink
