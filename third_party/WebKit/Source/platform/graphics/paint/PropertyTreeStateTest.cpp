// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/PropertyTreeState.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class PropertyTreeStateTest : public ::testing::Test {};

TEST_F(PropertyTreeStateTest, TransformOnEffectOnClip) {
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::create(TransformPaintPropertyNode::root(),
                                         TransformationMatrix(),
                                         FloatPoint3D());

  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::create(
      ClipPaintPropertyNode::root(), TransformPaintPropertyNode::root(),
      FloatRoundedRect());

  RefPtr<EffectPaintPropertyNode> effect = EffectPaintPropertyNode::create(
      EffectPaintPropertyNode::root(), TransformPaintPropertyNode::root(),
      clip.get(), ColorFilterNone, CompositorFilterOperations(), 1.0,
      SkBlendMode::kSrcOver);

  PropertyTreeState state(transform.get(), clip.get(), effect.get());
  EXPECT_EQ(PropertyTreeState::Transform, state.innermostNode());

  PropertyTreeStateIterator iterator(state);
  EXPECT_EQ(PropertyTreeState::Effect, iterator.next()->innermostNode());
  EXPECT_EQ(PropertyTreeState::Clip, iterator.next()->innermostNode());
  EXPECT_EQ(PropertyTreeState::None, iterator.next()->innermostNode());
}

TEST_F(PropertyTreeStateTest, RootState) {
  PropertyTreeState state(TransformPaintPropertyNode::root(),
                          ClipPaintPropertyNode::root(),
                          EffectPaintPropertyNode::root());
  EXPECT_EQ(PropertyTreeState::None, state.innermostNode());
}

TEST_F(PropertyTreeStateTest, EffectOnClipOnTransform) {
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::create(TransformPaintPropertyNode::root(),
                                         TransformationMatrix(),
                                         FloatPoint3D());

  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::create(
      ClipPaintPropertyNode::root(), transform.get(), FloatRoundedRect());

  RefPtr<EffectPaintPropertyNode> effect = EffectPaintPropertyNode::create(
      EffectPaintPropertyNode::root(), transform.get(), clip.get(),
      ColorFilterNone, CompositorFilterOperations(), 1.0,
      SkBlendMode::kSrcOver);

  PropertyTreeState state(transform.get(), clip.get(), effect.get());
  EXPECT_EQ(PropertyTreeState::Effect, state.innermostNode());

  PropertyTreeStateIterator iterator(state);
  EXPECT_EQ(PropertyTreeState::Clip, iterator.next()->innermostNode());
  EXPECT_EQ(PropertyTreeState::Transform, iterator.next()->innermostNode());
  EXPECT_EQ(PropertyTreeState::None, iterator.next()->innermostNode());
}

TEST_F(PropertyTreeStateTest, ClipOnEffectOnTransform) {
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::create(TransformPaintPropertyNode::root(),
                                         TransformationMatrix(),
                                         FloatPoint3D());

  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::create(
      ClipPaintPropertyNode::root(), transform.get(), FloatRoundedRect());

  RefPtr<EffectPaintPropertyNode> effect = EffectPaintPropertyNode::create(
      EffectPaintPropertyNode::root(), transform.get(),
      ClipPaintPropertyNode::root(), ColorFilterNone,
      CompositorFilterOperations(), 1.0, SkBlendMode::kSrcOver);

  PropertyTreeState state(transform.get(), clip.get(), effect.get());
  EXPECT_EQ(PropertyTreeState::Clip, state.innermostNode());

  PropertyTreeStateIterator iterator(state);
  EXPECT_EQ(PropertyTreeState::Effect, iterator.next()->innermostNode());
  EXPECT_EQ(PropertyTreeState::Transform, iterator.next()->innermostNode());
  EXPECT_EQ(PropertyTreeState::None, iterator.next()->innermostNode());
}

TEST_F(PropertyTreeStateTest, ClipDescendantOfTransform) {
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::create(TransformPaintPropertyNode::root(),
                                         TransformationMatrix(),
                                         FloatPoint3D());

  RefPtr<TransformPaintPropertyNode> transform2 =
      TransformPaintPropertyNode::create(
          transform.get(), TransformationMatrix(), FloatPoint3D());

  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::create(
      ClipPaintPropertyNode::root(), transform2.get(), FloatRoundedRect());

  RefPtr<EffectPaintPropertyNode> effect = EffectPaintPropertyNode::create(
      EffectPaintPropertyNode::root(), TransformPaintPropertyNode::root(),
      ClipPaintPropertyNode::root(), ColorFilterNone,
      CompositorFilterOperations(), 1.0, SkBlendMode::kSrcOver);

  // Here the clip is inside of its own transform, but the transform is an
  // ancestor of the clip's transform. This models situations such as
  // a clip inside a scroller that applies to an absolute-positioned element
  // which escapes the scroll transform but not the clip.
  PropertyTreeState state(transform.get(), clip.get(), effect.get());
  EXPECT_EQ(PropertyTreeState::Clip, state.innermostNode());

  PropertyTreeStateIterator iterator(state);
  EXPECT_EQ(PropertyTreeState::Transform, iterator.next()->innermostNode());
  EXPECT_EQ(PropertyTreeState::Effect, iterator.next()->innermostNode());
  EXPECT_EQ(PropertyTreeState::None, iterator.next()->innermostNode());
}

TEST_F(PropertyTreeStateTest, EffectDescendantOfTransform) {
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::create(TransformPaintPropertyNode::root(),
                                         TransformationMatrix(),
                                         FloatPoint3D());

  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::create(
      ClipPaintPropertyNode::root(), TransformPaintPropertyNode::root(),
      FloatRoundedRect());

  RefPtr<TransformPaintPropertyNode> transform2 =
      TransformPaintPropertyNode::create(TransformPaintPropertyNode::root(),
                                         TransformationMatrix(),
                                         FloatPoint3D());

  RefPtr<EffectPaintPropertyNode> effect = EffectPaintPropertyNode::create(
      EffectPaintPropertyNode::root(), transform2.get(), clip.get(),
      ColorFilterNone, CompositorFilterOperations(), 1.0,
      SkBlendMode::kSrcOver);

  // Here the clip is inside of its own transform, but the transform is an
  // ancestor of the clip's transform. This models situations such as
  // a clip inside a scroller that applies to an absolute-positioned element
  // which escapes the scroll transform but not the clip.
  PropertyTreeState state(transform.get(), clip.get(), effect.get());
  EXPECT_EQ(PropertyTreeState::Effect, state.innermostNode());

  PropertyTreeStateIterator iterator(state);
  EXPECT_EQ(PropertyTreeState::Transform, iterator.next()->innermostNode());
  EXPECT_EQ(PropertyTreeState::Clip, iterator.next()->innermostNode());
  EXPECT_EQ(PropertyTreeState::None, iterator.next()->innermostNode());
}

TEST_F(PropertyTreeStateTest, CompositorElementIdNoElementIdOnAnyNode) {
  PropertyTreeState state(TransformPaintPropertyNode::root(),
                          ClipPaintPropertyNode::root(),
                          EffectPaintPropertyNode::root());
  EXPECT_EQ(CompositorElementId(), state.compositorElementId());
}

TEST_F(PropertyTreeStateTest, CompositorElementIdWithElementIdOnTransformNode) {
  CompositorElementId expectedCompositorElementId = CompositorElementId(2, 0);
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::create(TransformPaintPropertyNode::root(),
                                         TransformationMatrix(), FloatPoint3D(),
                                         false, 0, CompositingReasonNone,
                                         expectedCompositorElementId);
  PropertyTreeState state(transform.get(), ClipPaintPropertyNode::root(),
                          EffectPaintPropertyNode::root());
  EXPECT_EQ(expectedCompositorElementId, state.compositorElementId());
}

TEST_F(PropertyTreeStateTest, CompositorElementIdWithElementIdOnEffectNode) {
  CompositorElementId expectedCompositorElementId = CompositorElementId(2, 0);
  RefPtr<EffectPaintPropertyNode> effect = EffectPaintPropertyNode::create(
      EffectPaintPropertyNode::root(), TransformPaintPropertyNode::root(),
      ClipPaintPropertyNode::root(), ColorFilterNone,
      CompositorFilterOperations(), 1.0, SkBlendMode::kSrcOver,
      CompositingReasonNone, expectedCompositorElementId);
  PropertyTreeState state(TransformPaintPropertyNode::root(),
                          ClipPaintPropertyNode::root(), effect.get());
  EXPECT_EQ(expectedCompositorElementId, state.compositorElementId());
}

TEST_F(PropertyTreeStateTest, CompositorElementIdWithElementIdOnMultipleNodes) {
  CompositorElementId expectedCompositorElementId = CompositorElementId(2, 0);
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::create(TransformPaintPropertyNode::root(),
                                         TransformationMatrix(), FloatPoint3D(),
                                         false, 0, CompositingReasonNone,
                                         expectedCompositorElementId);
  RefPtr<EffectPaintPropertyNode> effect = EffectPaintPropertyNode::create(
      EffectPaintPropertyNode::root(), TransformPaintPropertyNode::root(),
      ClipPaintPropertyNode::root(), ColorFilterNone,
      CompositorFilterOperations(), 1.0, SkBlendMode::kSrcOver,
      CompositingReasonNone, expectedCompositorElementId);
  PropertyTreeState state(transform.get(), ClipPaintPropertyNode::root(),
                          effect.get());
  EXPECT_EQ(expectedCompositorElementId, state.compositorElementId());
}

}  // namespace blink
