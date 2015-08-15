// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "core/layout/LayoutTestHelper.h"
#include "core/layout/LayoutView.h"
#include "core/paint/DeprecatedPaintLayer.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/transforms/TransformTestHelper.h"
#include "platform/transforms/TransformationMatrix.h"
#include <gtest/gtest.h>

namespace blink {
namespace {

class DisplayListCompositingTest : public RenderingTest {
public:
    DisplayListCompositingTest()
        : m_layoutView(nullptr)
        , m_originalSlimmingPaintV2Enabled(RuntimeEnabledFeatures::slimmingPaintV2Enabled()) { }

protected:
    LayoutView& layoutView() { return *m_layoutView; }

private:
    void SetUp() override
    {
        ASSERT_TRUE(RuntimeEnabledFeatures::slimmingPaintEnabled());
        RuntimeEnabledFeatures::setSlimmingPaintV2Enabled(true);

        RenderingTest::SetUp();
        enableCompositing();

        m_layoutView = document().view()->layoutView();
        ASSERT_TRUE(m_layoutView);
    }

    void TearDown() override
    {
        RuntimeEnabledFeatures::setSlimmingPaintV2Enabled(m_originalSlimmingPaintV2Enabled);
    }

    LayoutView* m_layoutView;
    bool m_originalSlimmingPaintV2Enabled;
};

TEST_F(DisplayListCompositingTest, TransformTreeBuildingNoTransforms)
{
    setBodyInnerHTML("<style>* { margin: 0; padding: 0;}</style><div>A</div>");
    const auto* compositedDisplayList = layoutView().compositedDisplayList();
    auto* tree = compositedDisplayList->transformTree.get();

    // There should only be a root transform node.
    ASSERT_EQ(1u, tree->nodeCount());
    EXPECT_TRUE(tree->nodeAt(0).isRoot());
    EXPECT_TRUE(tree->nodeAt(0).matrix.isIdentity());
}

TEST_F(DisplayListCompositingTest, TransformTreeBuildingMultipleTransforms)
{
    setBodyInnerHTML("<style>* { margin: 0; padding: 0;}</style><div style=\"transform: translate3d(2px,3px,4px);\">X</div>");
    const auto* compositedDisplayList = layoutView().compositedDisplayList();
    auto* tree = compositedDisplayList->transformTree.get();

    ASSERT_EQ(2u, tree->nodeCount());
    EXPECT_TRUE(tree->nodeAt(0).isRoot());
    EXPECT_FALSE(tree->nodeAt(1).isRoot());

    TransformationMatrix matrix;
    matrix.translate3d(2, 3, 4);
    EXPECT_TRANSFORMS_ALMOST_EQ(matrix, tree->nodeAt(1).matrix);

    // Add a nested 3d transform.
    setBodyInnerHTML("<style>* { margin: 0; padding: 0;}</style><div style=\"transform: translate3d(2px,3px,4px);\"><div style=\"transform: translate3d(5px,6px,7px);\">X</div></div>");
    compositedDisplayList = layoutView().compositedDisplayList();
    tree = compositedDisplayList->transformTree.get();

    ASSERT_EQ(3u, tree->nodeCount());
    EXPECT_TRUE(tree->nodeAt(0).isRoot());
    EXPECT_FALSE(tree->nodeAt(1).isRoot());
    EXPECT_FALSE(tree->nodeAt(2).isRoot());

    matrix.makeIdentity();
    matrix.translate3d(2, 3, 4);
    EXPECT_TRANSFORMS_ALMOST_EQ(matrix, tree->nodeAt(1).matrix);

    matrix.makeIdentity();
    matrix.translate3d(5, 6, 7);
    EXPECT_TRANSFORMS_ALMOST_EQ(matrix, tree->nodeAt(2).matrix);
}

}
} // namespace blink
