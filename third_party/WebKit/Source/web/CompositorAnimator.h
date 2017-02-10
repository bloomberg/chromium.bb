// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorAnimator_h
#define CompositorAnimator_h

#include "platform/graphics/CompositorMutableStateProvider.h"
#include "platform/heap/Handle.h"

namespace blink {

class CompositorAnimator : public GarbageCollectedMixin {
 public:
  // Runs the animation frame callback for the frame starting at the given time.
  // Returns true if another animation frame was requested (i.e. should be
  // reinvoked next frame).
  virtual bool mutate(double monotonicTimeNow,
                      CompositorMutableStateProvider*) = 0;
  DEFINE_INLINE_VIRTUAL_TRACE() {}
};

}  // namespace blink

#endif  // CompositorAnimator_h
