// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebOverscrollBehavior_h
#define WebOverscrollBehavior_h

#include "cc/input/overscroll_behavior.h"

using cc::OverscrollBehavior;

namespace blink {

// The scroll-boundary-behavior allows developers to specify whether the
// scroll should be propagated to its ancestors at the beginning of the scroll,
// and whether the overscroll should cause UI affordance such as glow/bounce
// etc. See cc/input/overscroll_behavior.h for detailed value description.
struct WebOverscrollBehavior : public OverscrollBehavior {
  using OverscrollBehaviorType = OverscrollBehavior::OverscrollBehaviorType;
  WebOverscrollBehavior() : OverscrollBehavior() {}
  WebOverscrollBehavior(OverscrollBehaviorType type)
      : OverscrollBehavior(type) {}
  WebOverscrollBehavior(OverscrollBehaviorType x_type,
                        OverscrollBehaviorType y_type)
      : OverscrollBehavior(x_type, y_type) {}
};

}  // namespace blink

#endif
