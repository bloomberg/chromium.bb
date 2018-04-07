// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/worklet_animation_controller.h"

#include "third_party/blink/renderer/core/animation/worklet_animation_base.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

WorkletAnimationController::WorkletAnimationController(Document* document)
    : document_(document) {}

WorkletAnimationController::~WorkletAnimationController() = default;

void WorkletAnimationController::AttachAnimation(
    WorkletAnimationBase& animation) {
  DCHECK(IsMainThread());
  DCHECK(!pending_animations_.Contains(&animation));
  DCHECK(!compositor_animations_.Contains(&animation));
  pending_animations_.insert(&animation);

  // TODO(majidvp): We should DCHECK that the animation document is the same
  // as the controller's owning document.
  if (LocalFrameView* view = animation.GetDocument()->View())
    view->ScheduleAnimation();
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
    String failure_message;
    if (animation->StartOnCompositor(&failure_message)) {
      compositor_animations_.insert(animation);
    } else {
      document_->AddConsoleMessage(ConsoleMessage::Create(
          kOtherMessageSource, kWarningMessageLevel, failure_message));
    }
  }
}

void WorkletAnimationController::Trace(blink::Visitor* visitor) {
  visitor->Trace(pending_animations_);
  visitor->Trace(compositor_animations_);
  visitor->Trace(document_);
}

}  // namespace blink
