// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/PropertyTreeState.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class PropertyTreeStateTest : public ::testing::Test {};

TEST_F(PropertyTreeStateTest, ClipBelowOutputClipOfEffect) {
  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::Create(
      ClipPaintPropertyNode::Root(), TransformPaintPropertyNode::Root(),
      FloatRoundedRect());

  RefPtr<EffectPaintPropertyNode> effect = EffectPaintPropertyNode::Create(
      EffectPaintPropertyNode::Root(), TransformPaintPropertyNode::Root(),
      ClipPaintPropertyNode::Root(), kColorFilterNone,
      CompositorFilterOperations(), 1.0, SkBlendMode::kSrcOver);

  PropertyTreeState state(TransformPaintPropertyNode::Root(), clip.Get(),
                          effect.Get());
  EXPECT_EQ(PropertyTreeState::kEffect, state.GetInnermostNode());

  PropertyTreeStateIterator iterator(state);
  EXPECT_EQ(PropertyTreeState::kClip, iterator.Next()->GetInnermostNode());
  EXPECT_EQ(PropertyTreeState::kNone, iterator.Next()->GetInnermostNode());
}

TEST_F(PropertyTreeStateTest, ClipBelowOutputClipOfEffectMovingPixels) {
  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::Create(
      ClipPaintPropertyNode::Root(), TransformPaintPropertyNode::Root(),
      FloatRoundedRect());

  CompositorFilterOperations operations;
  operations.AppendBlurFilter(2);
  RefPtr<EffectPaintPropertyNode> effect = EffectPaintPropertyNode::Create(
      EffectPaintPropertyNode::Root(), TransformPaintPropertyNode::Root(),
      ClipPaintPropertyNode::Root(), kColorFilterNone, operations, 1.0,
      SkBlendMode::kSrcOver);

  PropertyTreeState state(TransformPaintPropertyNode::Root(), clip.Get(),
                          effect.Get());
  // If the effect moves pixels, the clip must happen first.
  EXPECT_EQ(PropertyTreeState::kClip, state.GetInnermostNode());

  PropertyTreeStateIterator iterator(state);
  EXPECT_EQ(PropertyTreeState::kEffect, iterator.Next()->GetInnermostNode());
  EXPECT_EQ(PropertyTreeState::kNone, iterator.Next()->GetInnermostNode());
}

TEST_F(PropertyTreeStateTest, TransformOnEffectOnClip) {
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::Create(TransformPaintPropertyNode::Root(),
                                         TransformationMatrix(),
                                         FloatPoint3D());

  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::Create(
      ClipPaintPropertyNode::Root(), TransformPaintPropertyNode::Root(),
      FloatRoundedRect());

  RefPtr<EffectPaintPropertyNode> effect = EffectPaintPropertyNode::Create(
      EffectPaintPropertyNode::Root(), TransformPaintPropertyNode::Root(),
      clip.Get(), kColorFilterNone, CompositorFilterOperations(), 1.0,
      SkBlendMode::kSrcOver);

  PropertyTreeState state(transform.Get(), clip.Get(), effect.Get());
  EXPECT_EQ(PropertyTreeState::kTransform, state.GetInnermostNode());

  PropertyTreeStateIterator iterator(state);
  EXPECT_EQ(PropertyTreeState::kEffect, iterator.Next()->GetInnermostNode());
  EXPECT_EQ(PropertyTreeState::kClip, iterator.Next()->GetInnermostNode());
  EXPECT_EQ(PropertyTreeState::kNone, iterator.Next()->GetInnermostNode());
}

TEST_F(PropertyTreeStateTest, RootState) {
  PropertyTreeState state(TransformPaintPropertyNode::Root(),
                          ClipPaintPropertyNode::Root(),
                          EffectPaintPropertyNode::Root());
  EXPECT_EQ(PropertyTreeState::kNone, state.GetInnermostNode());
}

TEST_F(PropertyTreeStateTest, EffectOnClipOnTransform) {
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::Create(TransformPaintPropertyNode::Root(),
                                         TransformationMatrix(),
                                         FloatPoint3D());

  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::Create(
      ClipPaintPropertyNode::Root(), transform.Get(), FloatRoundedRect());

  RefPtr<EffectPaintPropertyNode> effect = EffectPaintPropertyNode::Create(
      EffectPaintPropertyNode::Root(), transform.Get(), clip.Get(),
      kColorFilterNone, CompositorFilterOperations(), 1.0,
      SkBlendMode::kSrcOver);

  PropertyTreeState state(transform.Get(), clip.Get(), effect.Get());
  EXPECT_EQ(PropertyTreeState::kEffect, state.GetInnermostNode());

  PropertyTreeStateIterator iterator(state);
  EXPECT_EQ(PropertyTreeState::kClip, iterator.Next()->GetInnermostNode());
  EXPECT_EQ(PropertyTreeState::kTransform, iterator.Next()->GetInnermostNode());
  EXPECT_EQ(PropertyTreeState::kNone, iterator.Next()->GetInnermostNode());
}

TEST_F(PropertyTreeStateTest, ClipOnEffectOnTransform) {
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::Create(TransformPaintPropertyNode::Root(),
                                         TransformationMatrix(),
                                         FloatPoint3D());

  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::Create(
      ClipPaintPropertyNode::Root(), transform.Get(), FloatRoundedRect());

  RefPtr<EffectPaintPropertyNode> effect = EffectPaintPropertyNode::Create(
      EffectPaintPropertyNode::Root(), transform.Get(),
      ClipPaintPropertyNode::Root(), kColorFilterNone,
      CompositorFilterOperations(), 1.0, SkBlendMode::kSrcOver);

  PropertyTreeState state(transform.Get(), clip.Get(), effect.Get());
  EXPECT_EQ(PropertyTreeState::kEffect, state.GetInnermostNode());

  PropertyTreeStateIterator iterator(state);
  EXPECT_EQ(PropertyTreeState::kClip, iterator.Next()->GetInnermostNode());
  EXPECT_EQ(PropertyTreeState::kTransform, iterator.Next()->GetInnermostNode());
  EXPECT_EQ(PropertyTreeState::kNone, iterator.Next()->GetInnermostNode());
}

TEST_F(PropertyTreeStateTest, ClipDescendantOfTransform) {
  // CSS transform
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::Create(TransformPaintPropertyNode::Root(),
                                         TransformationMatrix(),
                                         FloatPoint3D());

  // Scroll transform
  RefPtr<TransformPaintPropertyNode> transform2 =
      TransformPaintPropertyNode::Create(
          transform.Get(), TransformationMatrix(), FloatPoint3D());

  // CSS clip
  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::Create(
      ClipPaintPropertyNode::Root(), transform2.Get(), FloatRoundedRect());

  RefPtr<EffectPaintPropertyNode> effect = EffectPaintPropertyNode::Create(
      EffectPaintPropertyNode::Root(), TransformPaintPropertyNode::Root(),
      ClipPaintPropertyNode::Root(), kColorFilterNone,
      CompositorFilterOperations(), 1.0, SkBlendMode::kSrcOver);

  // Here the clip is inside of its own transform, but the
  // PropertyTreeState of the content is an ancestor of the clip's transform./
  // This models situations such as a CSS clip inside a scroller that applies to
  // an absolute-positioned element which escapes the scroll transforms but not
  // the clip.
  PropertyTreeState state(transform.Get(), clip.Get(), effect.Get());
  EXPECT_EQ(PropertyTreeState::kClip, state.GetInnermostNode());

  PropertyTreeStateIterator iterator(state);
  EXPECT_EQ(PropertyTreeState::kTransform, iterator.Next()->GetInnermostNode());
  EXPECT_EQ(PropertyTreeState::kEffect, iterator.Next()->GetInnermostNode());
  EXPECT_EQ(PropertyTreeState::kNone, iterator.Next()->GetInnermostNode());
}

TEST_F(PropertyTreeStateTest, EffectDescendantOfTransform) {
  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::Create(
      ClipPaintPropertyNode::Root(), TransformPaintPropertyNode::Root(),
      FloatRoundedRect());

  RefPtr<TransformPaintPropertyNode> transform2 =
      TransformPaintPropertyNode::Create(TransformPaintPropertyNode::Root(),
                                         TransformationMatrix(),
                                         FloatPoint3D());

  RefPtr<EffectPaintPropertyNode> effect = EffectPaintPropertyNode::Create(
      EffectPaintPropertyNode::Root(), transform2.Get(), clip.Get(),
      kColorFilterNone, CompositorFilterOperations(), 1.0,
      SkBlendMode::kSrcOver);

  PropertyTreeState state(transform2.Get(), clip.Get(), effect.Get());
  EXPECT_EQ(PropertyTreeState::kEffect, state.GetInnermostNode());

  PropertyTreeStateIterator iterator(state);
  EXPECT_EQ(PropertyTreeState::kTransform, iterator.Next()->GetInnermostNode());
  EXPECT_EQ(PropertyTreeState::kClip, iterator.Next()->GetInnermostNode());
  EXPECT_EQ(PropertyTreeState::kNone, iterator.Next()->GetInnermostNode());
}

TEST_F(PropertyTreeStateTest, CompositorElementIdNoElementIdOnAnyNode) {
  PropertyTreeState state(TransformPaintPropertyNode::Root(),
                          ClipPaintPropertyNode::Root(),
                          EffectPaintPropertyNode::Root());
  EXPECT_EQ(CompositorElementId(),
            state.GetCompositorElementId(CompositorElementIdSet()));
}

TEST_F(PropertyTreeStateTest, CompositorElementIdWithElementIdOnTransformNode) {
  CompositorElementId expected_compositor_element_id = CompositorElementId(2);
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::Create(TransformPaintPropertyNode::Root(),
                                         TransformationMatrix(), FloatPoint3D(),
                                         false, 0, kCompositingReasonNone,
                                         expected_compositor_element_id);
  PropertyTreeState state(transform.Get(), ClipPaintPropertyNode::Root(),
                          EffectPaintPropertyNode::Root());
  EXPECT_EQ(expected_compositor_element_id,
            state.GetCompositorElementId(CompositorElementIdSet()));
}

TEST_F(PropertyTreeStateTest, CompositorElementIdWithElementIdOnEffectNode) {
  CompositorElementId expected_compositor_element_id = CompositorElementId(2);
  RefPtr<EffectPaintPropertyNode> effect = EffectPaintPropertyNode::Create(
      EffectPaintPropertyNode::Root(), TransformPaintPropertyNode::Root(),
      ClipPaintPropertyNode::Root(), kColorFilterNone,
      CompositorFilterOperations(), 1.0, SkBlendMode::kSrcOver,
      kCompositingReasonNone, expected_compositor_element_id);
  PropertyTreeState state(TransformPaintPropertyNode::Root(),
                          ClipPaintPropertyNode::Root(), effect.Get());
  EXPECT_EQ(expected_compositor_element_id,
            state.GetCompositorElementId(CompositorElementIdSet()));
}

TEST_F(PropertyTreeStateTest, CompositorElementIdWithElementIdOnMultipleNodes) {
  CompositorElementId expected_compositor_element_id = CompositorElementId(2);
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::Create(TransformPaintPropertyNode::Root(),
                                         TransformationMatrix(), FloatPoint3D(),
                                         false, 0, kCompositingReasonNone,
                                         expected_compositor_element_id);
  RefPtr<EffectPaintPropertyNode> effect = EffectPaintPropertyNode::Create(
      EffectPaintPropertyNode::Root(), TransformPaintPropertyNode::Root(),
      ClipPaintPropertyNode::Root(), kColorFilterNone,
      CompositorFilterOperations(), 1.0, SkBlendMode::kSrcOver,
      kCompositingReasonNone, expected_compositor_element_id);
  PropertyTreeState state(transform.Get(), ClipPaintPropertyNode::Root(),
                          effect.Get());
  EXPECT_EQ(expected_compositor_element_id,
            state.GetCompositorElementId(CompositorElementIdSet()));
}

TEST_F(PropertyTreeStateTest, CompositorElementIdWithDifferingElementIds) {
  CompositorElementId first_compositor_element_id = CompositorElementId(2);
  CompositorElementId second_compositor_element_id = CompositorElementId(3);
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::Create(TransformPaintPropertyNode::Root(),
                                         TransformationMatrix(), FloatPoint3D(),
                                         false, 0, kCompositingReasonNone,
                                         first_compositor_element_id);
  RefPtr<EffectPaintPropertyNode> effect = EffectPaintPropertyNode::Create(
      EffectPaintPropertyNode::Root(), TransformPaintPropertyNode::Root(),
      ClipPaintPropertyNode::Root(), kColorFilterNone,
      CompositorFilterOperations(), 1.0, SkBlendMode::kSrcOver,
      kCompositingReasonNone, second_compositor_element_id);
  PropertyTreeState state(transform.Get(), ClipPaintPropertyNode::Root(),
                          effect.Get());

  CompositorElementIdSet composited_element_ids;
  composited_element_ids.insert(first_compositor_element_id);
  EXPECT_EQ(second_compositor_element_id,
            state.GetCompositorElementId(composited_element_ids));

  composited_element_ids.clear();
  composited_element_ids.insert(second_compositor_element_id);
  EXPECT_EQ(first_compositor_element_id,
            state.GetCompositorElementId(composited_element_ids));
}

}  // namespace blink
