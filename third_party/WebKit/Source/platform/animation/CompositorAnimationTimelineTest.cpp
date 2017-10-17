// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/CompositorAnimationTimeline.h"

#include <memory>
#include "base/memory/scoped_refptr.h"
#include "cc/animation/animation_host.h"
#include "platform/animation/CompositorAnimationHost.h"
#include "platform/animation/CompositorAnimationPlayer.h"
#include "platform/testing/CompositorTest.h"
#include "platform/testing/WebLayerTreeViewImplForTesting.h"

namespace blink {

class CompositorAnimationTimelineTest : public CompositorTest {};

TEST_F(CompositorAnimationTimelineTest,
       CompositorTimelineDeletionDetachesFromAnimationHost) {
  std::unique_ptr<CompositorAnimationTimeline> timeline =
      CompositorAnimationTimeline::Create();

  scoped_refptr<cc::AnimationTimeline> cc_timeline =
      timeline->GetAnimationTimeline();
  EXPECT_FALSE(cc_timeline->animation_host());

  WebLayerTreeViewImplForTesting layer_tree_view;
  CompositorAnimationHost compositor_animation_host(
      layer_tree_view.CompositorAnimationHost());

  compositor_animation_host.AddTimeline(*timeline);
  cc::AnimationHost* animation_host = cc_timeline->animation_host();
  EXPECT_TRUE(animation_host);
  EXPECT_TRUE(animation_host->GetTimelineById(cc_timeline->id()));

  // Delete CompositorAnimationTimeline while attached to host.
  timeline = nullptr;

  EXPECT_FALSE(cc_timeline->animation_host());
  EXPECT_FALSE(animation_host->GetTimelineById(cc_timeline->id()));
}

}  // namespace blink
