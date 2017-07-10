/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "platform/graphics/GraphicsLayer.h"

#include <memory>
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/animation/CompositorAnimation.h"
#include "platform/animation/CompositorAnimationHost.h"
#include "platform/animation/CompositorAnimationPlayer.h"
#include "platform/animation/CompositorAnimationPlayerClient.h"
#include "platform/animation/CompositorAnimationTimeline.h"
#include "platform/animation/CompositorFloatAnimationCurve.h"
#include "platform/animation/CompositorTargetProperty.h"
#include "platform/graphics/CompositorElementId.h"
#include "platform/scheduler/child/web_scheduler.h"
#include "platform/scroll/ScrollableArea.h"
#include "platform/testing/FakeGraphicsLayer.h"
#include "platform/testing/FakeGraphicsLayerClient.h"
#include "platform/testing/WebLayerTreeViewImplForTesting.h"
#include "platform/transforms/Matrix3DTransformOperation.h"
#include "platform/transforms/RotateTransformOperation.h"
#include "platform/transforms/TranslateTransformOperation.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebLayer.h"
#include "public/platform/WebLayerTreeView.h"
#include "public/platform/WebThread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class GraphicsLayerTest : public ::testing::Test {
 public:
  GraphicsLayerTest() {
    clip_layer_ = WTF::WrapUnique(new FakeGraphicsLayer(&client_));
    scroll_elasticity_layer_ = WTF::WrapUnique(new FakeGraphicsLayer(&client_));
    page_scale_layer_ = WTF::WrapUnique(new FakeGraphicsLayer(&client_));
    graphics_layer_ = WTF::WrapUnique(new FakeGraphicsLayer(&client_));
    clip_layer_->AddChild(scroll_elasticity_layer_.get());
    scroll_elasticity_layer_->AddChild(page_scale_layer_.get());
    page_scale_layer_->AddChild(graphics_layer_.get());
    graphics_layer_->PlatformLayer()->SetScrollable(
        clip_layer_->PlatformLayer()->Bounds());
    platform_layer_ = graphics_layer_->PlatformLayer();
    layer_tree_view_ = WTF::WrapUnique(new WebLayerTreeViewImplForTesting);
    DCHECK(layer_tree_view_);
    layer_tree_view_->SetRootLayer(*clip_layer_->PlatformLayer());
    WebLayerTreeView::ViewportLayers viewport_layers;
    viewport_layers.overscroll_elasticity =
        scroll_elasticity_layer_->PlatformLayer();
    viewport_layers.page_scale = page_scale_layer_->PlatformLayer();
    viewport_layers.inner_viewport_container = clip_layer_->PlatformLayer();
    viewport_layers.inner_viewport_scroll = graphics_layer_->PlatformLayer();
    layer_tree_view_->RegisterViewportLayers(viewport_layers);
    layer_tree_view_->SetViewportSize(WebSize(1, 1));
  }

  ~GraphicsLayerTest() override {
    graphics_layer_.reset();
    layer_tree_view_.reset();
  }

  WebLayerTreeView* LayerTreeView() { return layer_tree_view_.get(); }

 protected:
  WebLayer* platform_layer_;
  std::unique_ptr<FakeGraphicsLayer> graphics_layer_;
  std::unique_ptr<FakeGraphicsLayer> page_scale_layer_;
  std::unique_ptr<FakeGraphicsLayer> scroll_elasticity_layer_;
  std::unique_ptr<FakeGraphicsLayer> clip_layer_;

 private:
  std::unique_ptr<WebLayerTreeView> layer_tree_view_;
  FakeGraphicsLayerClient client_;
};

class AnimationPlayerForTesting : public CompositorAnimationPlayerClient {
 public:
  AnimationPlayerForTesting() {
    compositor_player_ = CompositorAnimationPlayer::Create();
  }

  CompositorAnimationPlayer* CompositorPlayer() const override {
    return compositor_player_.get();
  }

  std::unique_ptr<CompositorAnimationPlayer> compositor_player_;
};

TEST_F(GraphicsLayerTest, updateLayerShouldFlattenTransformWithAnimations) {
  ASSERT_FALSE(platform_layer_->HasTickingAnimationForTesting());

  std::unique_ptr<CompositorFloatAnimationCurve> curve =
      CompositorFloatAnimationCurve::Create();
  curve->AddKeyframe(
      CompositorFloatKeyframe(0.0, 0.0,
                              *CubicBezierTimingFunction::Preset(
                                  CubicBezierTimingFunction::EaseType::EASE)));
  std::unique_ptr<CompositorAnimation> float_animation(
      CompositorAnimation::Create(*curve, CompositorTargetProperty::OPACITY, 0,
                                  0));
  int animation_id = float_animation->Id();

  std::unique_ptr<CompositorAnimationTimeline> compositor_timeline =
      CompositorAnimationTimeline::Create();
  AnimationPlayerForTesting player;

  CompositorAnimationHost host(LayerTreeView()->CompositorAnimationHost());

  host.AddTimeline(*compositor_timeline);
  compositor_timeline->PlayerAttached(player);

  platform_layer_->SetElementId(CompositorElementId(platform_layer_->Id()));

  player.CompositorPlayer()->AttachElement(platform_layer_->GetElementId());
  ASSERT_TRUE(player.CompositorPlayer()->IsElementAttached());

  player.CompositorPlayer()->AddAnimation(std::move(float_animation));

  ASSERT_TRUE(platform_layer_->HasTickingAnimationForTesting());

  graphics_layer_->SetShouldFlattenTransform(false);

  platform_layer_ = graphics_layer_->PlatformLayer();
  ASSERT_TRUE(platform_layer_);

  ASSERT_TRUE(platform_layer_->HasTickingAnimationForTesting());
  player.CompositorPlayer()->RemoveAnimation(animation_id);
  ASSERT_FALSE(platform_layer_->HasTickingAnimationForTesting());

  graphics_layer_->SetShouldFlattenTransform(true);

  platform_layer_ = graphics_layer_->PlatformLayer();
  ASSERT_TRUE(platform_layer_);

  ASSERT_FALSE(platform_layer_->HasTickingAnimationForTesting());

  player.CompositorPlayer()->DetachElement();
  ASSERT_FALSE(player.CompositorPlayer()->IsElementAttached());

  compositor_timeline->PlayerDestroyed(player);
  host.RemoveTimeline(*compositor_timeline.get());
}

class FakeScrollableArea : public GarbageCollectedFinalized<FakeScrollableArea>,
                           public ScrollableArea {
  USING_GARBAGE_COLLECTED_MIXIN(FakeScrollableArea);

 public:
  static FakeScrollableArea* Create() { return new FakeScrollableArea; }

  bool IsActive() const override { return false; }
  int ScrollSize(ScrollbarOrientation) const override { return 100; }
  bool IsScrollCornerVisible() const override { return false; }
  IntRect ScrollCornerRect() const override { return IntRect(); }
  IntRect VisibleContentRect(
      IncludeScrollbarsInRect = kExcludeScrollbars) const override {
    return IntRect(ScrollOffsetInt().Width(), ScrollOffsetInt().Height(), 10,
                   10);
  }
  IntSize ContentsSize() const override { return IntSize(100, 100); }
  bool ScrollbarsCanBeActive() const override { return false; }
  IntRect ScrollableAreaBoundingBox() const override { return IntRect(); }
  void ScrollControlWasSetNeedsPaintInvalidation() override {}
  bool UserInputScrollable(ScrollbarOrientation) const override { return true; }
  bool ShouldPlaceVerticalScrollbarOnLeft() const override { return false; }
  int PageStep(ScrollbarOrientation) const override { return 0; }
  IntSize MinimumScrollOffsetInt() const override { return IntSize(); }
  IntSize MaximumScrollOffsetInt() const override {
    return ContentsSize() - IntSize(VisibleWidth(), VisibleHeight());
  }

  void UpdateScrollOffset(const ScrollOffset& offset, ScrollType) override {
    scroll_offset_ = offset;
  }
  ScrollOffset GetScrollOffset() const override { return scroll_offset_; }
  IntSize ScrollOffsetInt() const override {
    return FlooredIntSize(scroll_offset_);
  }

  RefPtr<WebTaskRunner> GetTimerTaskRunner() const final {
    return Platform::Current()->CurrentThread()->Scheduler()->TimerTaskRunner();
  }

  DEFINE_INLINE_VIRTUAL_TRACE() { ScrollableArea::Trace(visitor); }

 private:
  ScrollOffset scroll_offset_;
};

}  // namespace blink
