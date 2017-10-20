// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorMutator_h
#define CompositorMutator_h

#include "platform/PlatformExport.h"
#include "platform/graphics/CompositorAnimatorsState.h"
#include "platform/heap/Handle.h"

namespace blink {

class PLATFORM_EXPORT CompositorMutator
    : public GarbageCollectedFinalized<CompositorMutator> {
 public:
  virtual ~CompositorMutator() {}

  virtual void Trace(blink::Visitor* visitor) {}

  // Called from compositor thread to run the animation frame callbacks from all
  // connected AnimationWorklets.
  virtual void Mutate(double monotonic_time_now,
                      std::unique_ptr<CompositorMutatorInputState>) = 0;
  // Returns true if Mutate may do something if called 'now'.
  virtual bool HasAnimators() = 0;
};

}  // namespace blink

#endif  // CompositorMutator_h
