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
#include <utility>

#include "platform/animation/CompositorAnimation.h"
#include "platform/animation/CompositorAnimationClient.h"
#include "platform/animation/CompositorAnimationHost.h"
#include "platform/animation/CompositorAnimationTimeline.h"
#include "platform/animation/CompositorFloatAnimationCurve.h"
#include "platform/animation/CompositorKeyframeModel.h"
#include "platform/animation/CompositorTargetProperty.h"
#include "platform/graphics/CompositorElementId.h"
#include "platform/graphics/paint/PaintControllerTest.h"
#include "platform/graphics/paint/PropertyTreeState.h"
#include "platform/graphics/paint/ScopedPaintChunkProperties.h"
#include "platform/graphics/test/FakeScrollableArea.h"
#include "platform/scheduler/child/web_scheduler.h"
#include "platform/scroll/ScrollableArea.h"
#include "platform/testing/FakeGraphicsLayer.h"
#include "platform/testing/FakeGraphicsLayerClient.h"
#include "platform/testing/PaintTestConfigurations.h"
#include "platform/testing/WebLayerTreeViewImplForTesting.h"
#include "platform/transforms/Matrix3DTransformOperation.h"
#include "platform/transforms/RotateTransformOperation.h"
#include "platform/transforms/TranslateTransformOperation.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebLayer.h"
#include "public/platform/WebLayerTreeView.h"
#include "public/platform/WebThread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class GraphicsLayerTest : public testing::Test, public PaintTestConfigurations {
 public:
  GraphicsLayerTest() {
    clip_layer_ = std::make_unique<FakeGraphicsLayer>(client_);
    scroll_elasticity_layer_ = std::make_unique<FakeGraphicsLayer>(client_);
    page_scale_layer_ = std::make_unique<FakeGraphicsLayer>(client_);
    graphics_layer_ = std::make_unique<FakeGraphicsLayer>(client_);
    graphics_layer_->SetDrawsContent(true);
    clip_layer_->AddChild(scroll_elasticity_layer_.get());
    scroll_elasticity_layer_->AddChild(page_scale_layer_.get());
    page_scale_layer_->AddChild(graphics_layer_.get());
    graphics_layer_->PlatformLayer()->SetScrollable(
        clip_layer_->PlatformLayer()->Bounds());
    platform_layer_ = graphics_layer_->PlatformLayer();
    layer_tree_view_ = std::make_unique<WebLayerTreeViewImplForTesting>();
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

    if (RuntimeEnabledFeatures::SlimmingPaintV175Enabled()) {
      graphics_layer_->SetLayerState(
          PropertyTreeState(PropertyTreeState::Root()), IntPoint());
    }
  }

  ~GraphicsLayerTest() override {
    graphics_layer_.reset();
    layer_tree_view_.reset();
  }

  WebLayerTreeView* LayerTreeView() { return layer_tree_view_.get(); }

 protected:
  bool PaintWithoutCommit(GraphicsLayer& layer, const IntRect* interest_rect) {
    return layer.PaintWithoutCommit(interest_rect);
  }

  const CompositedLayerRasterInvalidator* GetInternalRasterInvalidator(
      const GraphicsLayer& layer) {
    return layer.raster_invalidator_.get();
  }

  CompositedLayerRasterInvalidator& EnsureRasterInvalidator(
      GraphicsLayer& layer) {
    return layer.EnsureRasterInvalidator();
  }

  const PaintController* GetInternalPaintController(
      const GraphicsLayer& layer) {
    return layer.paint_controller_.get();
  }

  WebLayer* platform_layer_;
  std::unique_ptr<FakeGraphicsLayer> graphics_layer_;
  std::unique_ptr<FakeGraphicsLayer> page_scale_layer_;
  std::unique_ptr<FakeGraphicsLayer> scroll_elasticity_layer_;
  std::unique_ptr<FakeGraphicsLayer> clip_layer_;
  FakeGraphicsLayerClient client_;

 private:
  std::unique_ptr<WebLayerTreeViewImplForTesting> layer_tree_view_;
};

INSTANTIATE_TEST_CASE_P(All,
                        GraphicsLayerTest,
                        testing::Values(0, kSlimmingPaintV175));

class AnimationForTesting : public CompositorAnimationClient {
 public:
  AnimationForTesting() {
    compositor_animation_ = CompositorAnimation::Create();
  }

  CompositorAnimation* GetCompositorAnimation() const override {
    return compositor_animation_.get();
  }

  std::unique_ptr<CompositorAnimation> compositor_animation_;
};

TEST_P(GraphicsLayerTest, updateLayerShouldFlattenTransformWithAnimations) {
  ASSERT_FALSE(platform_layer_->HasTickingAnimationForTesting());

  std::unique_ptr<CompositorFloatAnimationCurve> curve =
      CompositorFloatAnimationCurve::Create();
  curve->AddKeyframe(
      CompositorFloatKeyframe(0.0, 0.0,
                              *CubicBezierTimingFunction::Preset(
                                  CubicBezierTimingFunction::EaseType::EASE)));
  std::unique_ptr<CompositorKeyframeModel> float_keyframe_model(
      CompositorKeyframeModel::Create(*curve, CompositorTargetProperty::OPACITY,
                                      0, 0));
  int keyframe_model_id = float_keyframe_model->Id();

  std::unique_ptr<CompositorAnimationTimeline> compositor_timeline =
      CompositorAnimationTimeline::Create();
  AnimationForTesting animation;

  CompositorAnimationHost host(LayerTreeView()->CompositorAnimationHost());

  host.AddTimeline(*compositor_timeline);
  compositor_timeline->AnimationAttached(animation);

  platform_layer_->SetElementId(CompositorElementId(platform_layer_->Id()));

  animation.GetCompositorAnimation()->AttachElement(
      platform_layer_->GetElementId());
  ASSERT_TRUE(animation.GetCompositorAnimation()->IsElementAttached());

  animation.GetCompositorAnimation()->AddKeyframeModel(
      std::move(float_keyframe_model));

  ASSERT_TRUE(platform_layer_->HasTickingAnimationForTesting());

  graphics_layer_->SetShouldFlattenTransform(false);

  platform_layer_ = graphics_layer_->PlatformLayer();
  ASSERT_TRUE(platform_layer_);

  ASSERT_TRUE(platform_layer_->HasTickingAnimationForTesting());
  animation.GetCompositorAnimation()->RemoveKeyframeModel(keyframe_model_id);
  ASSERT_FALSE(platform_layer_->HasTickingAnimationForTesting());

  graphics_layer_->SetShouldFlattenTransform(true);

  platform_layer_ = graphics_layer_->PlatformLayer();
  ASSERT_TRUE(platform_layer_);

  ASSERT_FALSE(platform_layer_->HasTickingAnimationForTesting());

  animation.GetCompositorAnimation()->DetachElement();
  ASSERT_FALSE(animation.GetCompositorAnimation()->IsElementAttached());

  compositor_timeline->AnimationDestroyed(animation);
  host.RemoveTimeline(*compositor_timeline.get());
}

TEST_P(GraphicsLayerTest, Paint) {
  IntRect interest_rect(1, 2, 3, 4);
  EXPECT_TRUE(PaintWithoutCommit(*graphics_layer_, &interest_rect));
  graphics_layer_->GetPaintController().CommitNewDisplayItems();

  client_.SetNeedsRepaint(true);
  EXPECT_TRUE(PaintWithoutCommit(*graphics_layer_, &interest_rect));
  graphics_layer_->GetPaintController().CommitNewDisplayItems();

  client_.SetNeedsRepaint(false);
  EXPECT_FALSE(PaintWithoutCommit(*graphics_layer_, &interest_rect));

  interest_rect.Move(IntSize(10, 20));
  EXPECT_TRUE(PaintWithoutCommit(*graphics_layer_, &interest_rect));
  graphics_layer_->GetPaintController().CommitNewDisplayItems();
  EXPECT_FALSE(PaintWithoutCommit(*graphics_layer_, &interest_rect));

  graphics_layer_->SetNeedsDisplay();
  EXPECT_TRUE(PaintWithoutCommit(*graphics_layer_, &interest_rect));
  graphics_layer_->GetPaintController().CommitNewDisplayItems();
  EXPECT_FALSE(PaintWithoutCommit(*graphics_layer_, &interest_rect));
}

TEST_P(GraphicsLayerTest, PaintRecursively) {
  if (!RuntimeEnabledFeatures::SlimmingPaintV175Enabled())
    return;

  IntRect interest_rect(1, 2, 3, 4);
  auto transform_root = TransformPaintPropertyNode::Root();
  auto transform1 = TransformPaintPropertyNode::Create(
      transform_root, TransformationMatrix().Translate(10, 20), FloatPoint3D());
  auto transform2 = TransformPaintPropertyNode::Create(
      transform1, TransformationMatrix().Scale(2), FloatPoint3D());

  client_.SetPainter([&](const GraphicsLayer* layer, GraphicsContext& context,
                         GraphicsLayerPaintingPhase, const IntRect&) {
    {
      ScopedPaintChunkProperties properties(
          context.GetPaintController(), transform1, *layer, kBackgroundType);
      PaintControllerTestBase::DrawRect(context, *layer, kBackgroundType,
                                        interest_rect);
    }
    {
      ScopedPaintChunkProperties properties(
          context.GetPaintController(), transform2, *layer, kForegroundType);
      PaintControllerTestBase::DrawRect(context, *layer, kForegroundType,
                                        interest_rect);
    }
  });

  transform1->Update(transform_root, TransformationMatrix().Translate(20, 30),
                     FloatPoint3D());
  EXPECT_TRUE(transform1->Changed(*transform_root));
  EXPECT_TRUE(transform2->Changed(*transform_root));
  client_.SetNeedsRepaint(true);
  graphics_layer_->PaintRecursively();

  EXPECT_FALSE(transform1->Changed(*transform_root));
  EXPECT_FALSE(transform2->Changed(*transform_root));
}

TEST_P(GraphicsLayerTest, SetDrawsContentFalse) {
  EXPECT_TRUE(graphics_layer_->DrawsContent());
  graphics_layer_->GetPaintController();
  EXPECT_NE(nullptr, GetInternalPaintController(*graphics_layer_));
  EnsureRasterInvalidator(*graphics_layer_);
  EXPECT_NE(nullptr, GetInternalRasterInvalidator(*graphics_layer_));

  graphics_layer_->SetDrawsContent(false);
  EXPECT_EQ(nullptr, GetInternalPaintController(*graphics_layer_));
  EXPECT_EQ(nullptr, GetInternalRasterInvalidator(*graphics_layer_));
}

}  // namespace blink
