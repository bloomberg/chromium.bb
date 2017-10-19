// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/WorkletAnimationController.h"

#include "core/animation/WorkletAnimationBase.h"

namespace blink {

WorkletAnimationController::WorkletAnimationController() = default;

WorkletAnimationController::~WorkletAnimationController() = default;

void WorkletAnimationController::AttachAnimation(
    WorkletAnimationBase& animation) {
  DCHECK(IsMainThread());
  // TODO(smcgruer): Call NeedsCompositingUpdate on the relevant LocalFrameView.
  DCHECK(!pending_animations_.Contains(&animation));
  DCHECK(!compositor_animations_.Contains(&animation));
  pending_animations_.insert(&animation);
}

void WorkletAnimationController::DetachAnimation(
    WorkletAnimationBase& animation) {
  DCHECK(IsMainThread());
  DCHECK(pending_animations_.Contains(&animation) !=
         compositor_animations_.Contains(&animation));
  if (pending_animations_.Contains(&animation))
    pending_animations_.erase(&animation);
  else
    compositor_animations_.erase(&animation);
}

void WorkletAnimationController::Update() {
  DCHECK(IsMainThread());
  HeapHashSet<Member<WorkletAnimationBase>> animations;
  animations.swap(pending_animations_);
  for (const auto& animation : animations) {
    if (animation->StartOnCompositor()) {
      compositor_animations_.insert(animation);
    }
    // TODO(smcgruer): On failure, warn user. Perhaps fire cancel event?
  }
}

void WorkletAnimationController::Trace(blink::Visitor* visitor) {
  visitor->Trace(pending_animations_);
  visitor->Trace(compositor_animations_);
}

}  // namespace blink
