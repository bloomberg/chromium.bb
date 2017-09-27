// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorMutator_h
#define CompositorMutator_h

#include "platform/PlatformExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class PLATFORM_EXPORT CompositorMutator
    : public GarbageCollectedFinalized<CompositorMutator> {
 public:
  virtual ~CompositorMutator() {}

  DEFINE_INLINE_VIRTUAL_TRACE() {}

  // Called from compositor thread to run the animation frame callbacks from all
  // connected AnimationWorklets.
  // Returns true if any animation callbacks requested an animation frame
  // (i.e. should be reinvoked next frame).
  virtual bool Mutate(double monotonic_time_now) = 0;
};

}  // namespace blink

#endif  // CompositorMutator_h
