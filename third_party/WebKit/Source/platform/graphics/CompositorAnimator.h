// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorAnimator_h
#define CompositorAnimator_h

#include "platform/PlatformExport.h"
#include "platform/graphics/CompositorAnimatorsState.h"
#include "platform/heap/Handle.h"

namespace blink {

class PLATFORM_EXPORT CompositorAnimator : public GarbageCollectedMixin {
 public:
  // Runs the animation frame callback.
  virtual void Mutate(const CompositorMutatorInputState&) = 0;
  virtual void Trace(blink::Visitor* visitor) {}
};

}  // namespace blink

#endif  // CompositorAnimator_h
