// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/CompositorAnimationTimeline.h"

#include "cc/animation/animation_host.h"
#include "cc/animation/animation_id_provider.h"
#include "platform/animation/CompositorAnimationHost.h"
#include "platform/animation/CompositorAnimationPlayer.h"
#include "platform/animation/CompositorAnimationPlayerClient.h"

namespace blink {

CompositorAnimationTimeline::CompositorAnimationTimeline()
    : animation_timeline_(cc::AnimationTimeline::Create(
          cc::AnimationIdProvider::NextTimelineId())) {}

CompositorAnimationTimeline::~CompositorAnimationTimeline() {
  // Detach timeline from host, otherwise it stays there (leaks) until
  // compositor shutdown.
  if (animation_timeline_->animation_host())
    animation_timeline_->animation_host()->RemoveAnimationTimeline(
        animation_timeline_);
}

cc::AnimationTimeline* CompositorAnimationTimeline::GetAnimationTimeline()
    const {
  return animation_timeline_.get();
}

void CompositorAnimationTimeline::PlayerAttached(
    const blink::CompositorAnimationPlayerClient& client) {
  if (client.CompositorPlayer())
    animation_timeline_->AttachPlayer(
        client.CompositorPlayer()->CcAnimationPlayer());
}

void CompositorAnimationTimeline::PlayerDestroyed(
    const blink::CompositorAnimationPlayerClient& client) {
  if (client.CompositorPlayer())
    animation_timeline_->DetachPlayer(
        client.CompositorPlayer()->CcAnimationPlayer());
}

}  // namespace blink
