// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_TO_CC_ANIMATION_DELEGATE_ADAPTER_H_
#define WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_TO_CC_ANIMATION_DELEGATE_ADAPTER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "cc/animation/animation_delegate.h"

namespace blink { class WebAnimationDelegate; }

namespace webkit {

class WebToCCAnimationDelegateAdapter : public cc::AnimationDelegate {
 public:
  explicit WebToCCAnimationDelegateAdapter(
      blink::WebAnimationDelegate* delegate);

 private:
  virtual void NotifyAnimationStarted(
      double wall_clock_time,
      base::TimeTicks monotonic_time,
      cc::Animation::TargetProperty target_property) OVERRIDE;
  virtual void NotifyAnimationFinished(
      double wall_clock_time,
      base::TimeTicks monotonic_time,
      cc::Animation::TargetProperty target_property) OVERRIDE;

  blink::WebAnimationDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(WebToCCAnimationDelegateAdapter);
};

}  // namespace webkit

#endif  // WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_TO_CC_ANIMATION_DELEGATE_ADAPTER_H_
