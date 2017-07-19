// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebScrollBoundaryBehavior_h
#define WebScrollBoundaryBehavior_h

#include "cc/input/scroll_boundary_behavior.h"

using cc::ScrollBoundaryBehavior;

namespace blink {

// The scroll-boundary-behavior allows developers to specify whether the
// scroll should be propagated to its ancestors at the beginning of the scroll,
// and whether the overscroll should cause UI affordance such as glow/bounce
// etc. See cc/input/scroll_boundary_behavior.h for detailed value description.
struct WebScrollBoundaryBehavior : public ScrollBoundaryBehavior {
  using ScrollBoundaryBehaviorType =
      ScrollBoundaryBehavior::ScrollBoundaryBehaviorType;
  WebScrollBoundaryBehavior() : ScrollBoundaryBehavior() {}
  WebScrollBoundaryBehavior(ScrollBoundaryBehaviorType type)
      : ScrollBoundaryBehavior(type) {}
  WebScrollBoundaryBehavior(ScrollBoundaryBehaviorType x_type,
                            ScrollBoundaryBehaviorType y_type)
      : ScrollBoundaryBehavior(x_type, y_type) {}
};

}  // namespace blink

#endif
