// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/PropertyTreeState.h"

#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/paint/ClipPaintPropertyNode.h"
#include "platform/graphics/paint/EffectPaintPropertyNode.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class PropertyTreeStateTest : public ::testing::Test {
public:
    RefPtr<TransformPaintPropertyNode> rootTransformNode;
    RefPtr<ClipPaintPropertyNode> rootClipNode;
    RefPtr<EffectPaintPropertyNode> rootEffectNode;

    PropertyTreeState rootPropertyTreeState()
    {
        PropertyTreeState state(rootTransformNode.get(), rootClipNode.get(), rootEffectNode.get());
        return state;
    }

private:
    void SetUp() override
    {
        rootTransformNode = TransformPaintPropertyNode::create(TransformationMatrix(), FloatPoint3D(), nullptr);
        rootClipNode = ClipPaintPropertyNode::create(rootTransformNode, FloatRoundedRect(LayoutRect::infiniteIntRect()), nullptr);
        rootEffectNode = EffectPaintPropertyNode::create(1.0, nullptr);
    }
};

TEST_F(PropertyTreeStateTest, LeastCommonAncestor)
{
    TransformationMatrix matrix;
    RefPtr<TransformPaintPropertyNode> child1 = TransformPaintPropertyNode::create(matrix, FloatPoint3D(), rootPropertyTreeState().transform);
    RefPtr<TransformPaintPropertyNode> child2 = TransformPaintPropertyNode::create(matrix, FloatPoint3D(), rootPropertyTreeState().transform);

    RefPtr<TransformPaintPropertyNode> childOfChild1 = TransformPaintPropertyNode::create(matrix, FloatPoint3D(), child1);
    RefPtr<TransformPaintPropertyNode> childOfChild2 = TransformPaintPropertyNode::create(matrix, FloatPoint3D(), child2);

    EXPECT_EQ(rootPropertyTreeState().transform, propertyTreeNearestCommonAncestor<TransformPaintPropertyNode>(childOfChild1.get(), childOfChild2.get()));
    EXPECT_EQ(rootPropertyTreeState().transform, propertyTreeNearestCommonAncestor<TransformPaintPropertyNode>(childOfChild1.get(), child2.get()));
    EXPECT_EQ(rootPropertyTreeState().transform, propertyTreeNearestCommonAncestor<TransformPaintPropertyNode>(childOfChild1.get(), rootPropertyTreeState().transform.get()));
    EXPECT_EQ(child1, propertyTreeNearestCommonAncestor<TransformPaintPropertyNode>(childOfChild1.get(), child1.get()));

    EXPECT_EQ(rootPropertyTreeState().transform, propertyTreeNearestCommonAncestor<TransformPaintPropertyNode>(childOfChild2.get(), childOfChild1.get()));
    EXPECT_EQ(rootPropertyTreeState().transform, propertyTreeNearestCommonAncestor<TransformPaintPropertyNode>(childOfChild2.get(), child1.get()));
    EXPECT_EQ(rootPropertyTreeState().transform, propertyTreeNearestCommonAncestor<TransformPaintPropertyNode>(childOfChild2.get(), rootPropertyTreeState().transform.get()));
    EXPECT_EQ(child2, propertyTreeNearestCommonAncestor<TransformPaintPropertyNode>(childOfChild2.get(), child2.get()));

    EXPECT_EQ(rootPropertyTreeState().transform, propertyTreeNearestCommonAncestor<TransformPaintPropertyNode>(child1.get(), child2.get()));
}

} // namespace blink
