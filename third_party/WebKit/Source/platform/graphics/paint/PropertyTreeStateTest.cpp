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
        rootTransformNode = TransformPaintPropertyNode::create(nullptr, TransformationMatrix(), FloatPoint3D());
        rootClipNode = ClipPaintPropertyNode::create(nullptr, rootTransformNode, FloatRoundedRect(LayoutRect::infiniteIntRect()));
        rootEffectNode = EffectPaintPropertyNode::create(nullptr, 1.0);
    }
};

TEST_F(PropertyTreeStateTest, LeastCommonAncestor)
{
    TransformationMatrix matrix;
    RefPtr<TransformPaintPropertyNode> child1 = TransformPaintPropertyNode::create(rootPropertyTreeState().transform, matrix, FloatPoint3D());
    RefPtr<TransformPaintPropertyNode> child2 = TransformPaintPropertyNode::create(rootPropertyTreeState().transform, matrix, FloatPoint3D());

    RefPtr<TransformPaintPropertyNode> childOfChild1 = TransformPaintPropertyNode::create(child1, matrix, FloatPoint3D());
    RefPtr<TransformPaintPropertyNode> childOfChild2 = TransformPaintPropertyNode::create(child2, matrix, FloatPoint3D());

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
