// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/PropertyTreeState.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class PropertyTreeStateTest : public ::testing::Test {};

TEST_F(PropertyTreeStateTest, TrasformOnEffectOnClip) {
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::create(TransformPaintPropertyNode::root(),
                                         TransformationMatrix(),
                                         FloatPoint3D());

  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::create(
      ClipPaintPropertyNode::root(), TransformPaintPropertyNode::root(),
      FloatRoundedRect());

  RefPtr<EffectPaintPropertyNode> effect = EffectPaintPropertyNode::create(
      EffectPaintPropertyNode::root(), TransformPaintPropertyNode::root(),
      clip.get(), CompositorFilterOperations(), 1.0, SkBlendMode::kSrcOver);

  PropertyTreeState state(transform.get(), clip.get(), effect.get(),
                          ScrollPaintPropertyNode::root());
  EXPECT_EQ(PropertyTreeState::Transform, state.innermostNode());

  PropertyTreeStateIterator iterator(state);
  EXPECT_EQ(PropertyTreeState::Effect, iterator.next()->innermostNode());
  EXPECT_EQ(PropertyTreeState::Clip, iterator.next()->innermostNode());
  EXPECT_EQ(PropertyTreeState::None, iterator.next()->innermostNode());
}

TEST_F(PropertyTreeStateTest, RootState) {
  PropertyTreeState state(
      TransformPaintPropertyNode::root(), ClipPaintPropertyNode::root(),
      EffectPaintPropertyNode::root(), ScrollPaintPropertyNode::root());
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
      CompositorFilterOperations(), 1.0, SkBlendMode::kSrcOver);

  PropertyTreeState state(transform.get(), clip.get(), effect.get(),
                          ScrollPaintPropertyNode::root());
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
      ClipPaintPropertyNode::root(), CompositorFilterOperations(), 1.0,
      SkBlendMode::kSrcOver);

  PropertyTreeState state(transform.get(), clip.get(), effect.get(),
                          ScrollPaintPropertyNode::root());
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
      ClipPaintPropertyNode::root(), CompositorFilterOperations(), 1.0,
      SkBlendMode::kSrcOver);

  // Here the clip is inside of its own transform, but the transform is an
  // ancestor of the clip's transform. This models situations such as
  // a clip inside a scroller that applies to an absolute-positioned element
  // which escapes the scroll transform but not the clip.
  PropertyTreeState state(transform.get(), clip.get(), effect.get(),
                          ScrollPaintPropertyNode::root());
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
      CompositorFilterOperations(), 1.0, SkBlendMode::kSrcOver);

  // Here the clip is inside of its own transform, but the transform is an
  // ancestor of the clip's transform. This models situations such as
  // a clip inside a scroller that applies to an absolute-positioned element
  // which escapes the scroll transform but not the clip.
  PropertyTreeState state(transform.get(), clip.get(), effect.get(),
                          ScrollPaintPropertyNode::root());
  EXPECT_EQ(PropertyTreeState::Effect, state.innermostNode());

  PropertyTreeStateIterator iterator(state);
  EXPECT_EQ(PropertyTreeState::Transform, iterator.next()->innermostNode());
  EXPECT_EQ(PropertyTreeState::Clip, iterator.next()->innermostNode());
  EXPECT_EQ(PropertyTreeState::None, iterator.next()->innermostNode());
}

}  // namespace blink
