// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "cc/layers/picture_layer.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/mutator_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation_client.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation_host.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation_timeline.h"
#include "third_party/blink/renderer/platform/animation/compositor_float_animation_curve.h"
#include "third_party/blink/renderer/platform/animation/compositor_keyframe_model.h"
#include "third_party/blink/renderer/platform/animation/compositor_target_property.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/testing/fake_graphics_layer.h"
#include "third_party/blink/renderer/platform/testing/fake_graphics_layer_client.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/blink/renderer/platform/testing/viewport_layers_setup.h"

namespace blink {

class AnimatedLayersTest : public testing::Test,
                           public PaintTestConfigurations {
 public:
  AnimatedLayersTest() = default;
  ~AnimatedLayersTest() = default;

 protected:
  ViewportLayersSetup layers_;
};

INSTANTIATE_TEST_CASE_P(All,
                        AnimatedLayersTest,
                        testing::Values(0, kBlinkGenPropertyTrees));

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

TEST_P(AnimatedLayersTest, updateLayerShouldFlattenTransformWithAnimations) {
  // TODO(bokan): This test doesn't yet work in blink-gen-property-trees
  // because cc::Layers can't set an element id in that mode. We fail at
  // AttachElement since the element id is invalid. https://crbug.com/836897.
  if (RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled())
    return;

  cc::Layer* cc_layer = layers_.graphics_layer().CcLayer();
  cc::MutatorHost* mutator = layers_.layer_tree_host()->mutator_host();
  EXPECT_FALSE(
      mutator->HasTickingKeyframeModelForTesting(cc_layer->element_id()));

  std::unique_ptr<CompositorFloatAnimationCurve> curve =
      CompositorFloatAnimationCurve::Create();
  curve->AddKeyframe(
      CompositorFloatKeyframe(0.0, 0.0,
                              *CubicBezierTimingFunction::Preset(
                                  CubicBezierTimingFunction::EaseType::EASE)));
  std::unique_ptr<CompositorKeyframeModel> float_keyframe_model(
      CompositorKeyframeModel::Create(
          *curve, compositor_target_property::OPACITY, 0, 0));
  int keyframe_model_id = float_keyframe_model->Id();

  std::unique_ptr<CompositorAnimationTimeline> compositor_timeline =
      CompositorAnimationTimeline::Create();
  AnimationForTesting animation;

  CompositorAnimationHost host(layers_.animation_host());

  host.AddTimeline(*compositor_timeline);
  compositor_timeline->AnimationAttached(animation);

  cc_layer->SetElementId(CompositorElementId(cc_layer->id()));

  animation.GetCompositorAnimation()->AttachElement(cc_layer->element_id());
  ASSERT_TRUE(animation.GetCompositorAnimation()->IsElementAttached());

  animation.GetCompositorAnimation()->AddKeyframeModel(
      std::move(float_keyframe_model));

  EXPECT_TRUE(
      mutator->HasTickingKeyframeModelForTesting(cc_layer->element_id()));

  layers_.graphics_layer().SetShouldFlattenTransform(false);

  cc_layer = layers_.graphics_layer().CcLayer();
  ASSERT_TRUE(cc_layer);

  EXPECT_TRUE(
      mutator->HasTickingKeyframeModelForTesting(cc_layer->element_id()));
  animation.GetCompositorAnimation()->RemoveKeyframeModel(keyframe_model_id);
  EXPECT_FALSE(
      mutator->HasTickingKeyframeModelForTesting(cc_layer->element_id()));

  layers_.graphics_layer().SetShouldFlattenTransform(true);

  cc_layer = layers_.graphics_layer().CcLayer();
  ASSERT_TRUE(cc_layer);

  EXPECT_FALSE(
      mutator->HasTickingKeyframeModelForTesting(cc_layer->element_id()));

  animation.GetCompositorAnimation()->DetachElement();
  ASSERT_FALSE(animation.GetCompositorAnimation()->IsElementAttached());

  compositor_timeline->AnimationDestroyed(animation);
  host.RemoveTimeline(*compositor_timeline.get());
}

}  // namespace blink
