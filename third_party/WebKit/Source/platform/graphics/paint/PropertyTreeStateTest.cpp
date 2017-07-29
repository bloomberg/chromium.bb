// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/PropertyTreeState.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class PropertyTreeStateTest : public ::testing::Test {};

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
