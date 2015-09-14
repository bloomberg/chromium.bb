// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/compositing/DisplayListCompositingBuilder.h"

#include "platform/RuntimeEnabledFeatures.h"
#include <gtest/gtest.h>

namespace blink {

class DisplayListCompositingBuilderTest : public ::testing::Test {
public:
    DisplayListCompositingBuilderTest()
        : m_originalSlimmingPaintV2Enabled(RuntimeEnabledFeatures::slimmingPaintV2Enabled()) { }

private:
    void SetUp() override
    {
        RuntimeEnabledFeatures::setSlimmingPaintV2Enabled(true);
    }

    void TearDown() override
    {
        RuntimeEnabledFeatures::setSlimmingPaintV2Enabled(m_originalSlimmingPaintV2Enabled);
    }

    bool m_originalSlimmingPaintV2Enabled;
};

TEST_F(DisplayListCompositingBuilderTest, BasicTransformPropertyTree)
{
    OwnPtr<DisplayItemList> displayItemList = DisplayItemList::create();
    DisplayListDiff displayListDiff;
    displayItemList->commitNewDisplayItems(&displayListDiff);

    DisplayListCompositingBuilder compositingBuilder(*displayItemList, displayListDiff);
    CompositedDisplayList compositedDisplayList;
    compositingBuilder.build(compositedDisplayList);

    const auto& tree = *compositedDisplayList.transformTree;
    ASSERT_EQ(1u, tree.nodeCount());
    EXPECT_TRUE(tree.nodeAt(0).isRoot());
    EXPECT_TRUE(tree.nodeAt(0).matrix.isIdentity());
}

} // namespace blink
