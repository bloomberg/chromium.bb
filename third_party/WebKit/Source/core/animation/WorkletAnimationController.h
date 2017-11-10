// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkletAnimationController_h
#define WorkletAnimationController_h

#include "core/CoreExport.h"
#include "platform/heap/GarbageCollected.h"
#include "platform/heap/HeapAllocator.h"
#include "platform/heap/Visitor.h"

namespace blink {

class WorkletAnimationBase;

// Handles AnimationWorklet animations on the main-thread.
//
// The WorkletAnimationController is responsible for owning WorkletAnimation
// instances are long as they are relevant to the animation system. It is also
// responsible for starting valid WorkletAnimations on the compositor side and
// updating WorkletAnimations with updated results from their underpinning
// AnimationWorklet animator instance.
//
// For more details on AnimationWorklet, see the spec:
// https://wicg.github.io/animation-worklet
class CORE_EXPORT WorkletAnimationController
    : public GarbageCollectedFinalized<WorkletAnimationController> {
 public:
  WorkletAnimationController();
  virtual ~WorkletAnimationController();

  void AttachAnimation(WorkletAnimationBase&);
  void DetachAnimation(WorkletAnimationBase&);

  void Update();

  void Trace(blink::Visitor*);

 private:
  HeapHashSet<Member<WorkletAnimationBase>> pending_animations_;
  HeapHashSet<Member<WorkletAnimationBase>> compositor_animations_;
};

}  // namespace blink

#endif  // WorkletAnimationController_h
