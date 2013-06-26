// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/renderer/compositor_bindings/web_to_cc_animation_delegate_adapter.h"

#include "third_party/WebKit/public/platform/WebAnimationDelegate.h"

namespace webkit {

WebToCCAnimationDelegateAdapter::WebToCCAnimationDelegateAdapter(
    WebKit::WebAnimationDelegate* delegate)
    : delegate_(delegate) {}

void WebToCCAnimationDelegateAdapter::NotifyAnimationStarted(double time) {
  delegate_->notifyAnimationStarted(time);
}

void WebToCCAnimationDelegateAdapter::NotifyAnimationFinished(double time) {
  delegate_->notifyAnimationFinished(time);
}

}  // namespace webkit
