// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/CompositorAnimationHost.h"

#include "cc/animation/animation_host.h"
#include "cc/animation/scroll_offset_animations.h"
#include "platform/animation/CompositorAnimationTimeline.h"

namespace blink {

CompositorAnimationHost::CompositorAnimationHost(cc::AnimationHost* host)
    : m_animationHost(host) {
  DCHECK(m_animationHost);
}

void CompositorAnimationHost::addTimeline(
    const CompositorAnimationTimeline& timeline) {
  m_animationHost->AddAnimationTimeline(timeline.animationTimeline());
}

void CompositorAnimationHost::removeTimeline(
    const CompositorAnimationTimeline& timeline) {
  m_animationHost->RemoveAnimationTimeline(timeline.animationTimeline());
}

void CompositorAnimationHost::adjustImplOnlyScrollOffsetAnimation(
    CompositorElementId elementId,
    const gfx::Vector2dF& adjustment) {
  m_animationHost->scroll_offset_animations().AddAdjustmentUpdate(elementId,
                                                                  adjustment);
}

void CompositorAnimationHost::takeOverImplOnlyScrollOffsetAnimation(
    CompositorElementId elementId) {
  m_animationHost->scroll_offset_animations().AddTakeoverUpdate(elementId);
}

}  // namespace blink
