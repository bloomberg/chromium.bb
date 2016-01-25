// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutBlock.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/paint/PaintLayer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class PaintContainmentTest : public RenderingTest {
};

static void checkIsClippingStackingContextAndContainer(LayoutBoxModelObject& obj)
{
    EXPECT_TRUE(obj.canContainFixedPositionObjects());
    EXPECT_TRUE(obj.hasClipRelatedProperty());
    EXPECT_TRUE(obj.style()->containsPaint());

    // TODO(leviw): Ideally, we wouldn't require a paint layer to handle the clipping
    // and stacking performed by paint containment.
    ASSERT(obj.layer());
    PaintLayer* layer = obj.layer();
    EXPECT_TRUE(layer->stackingNode() && layer->stackingNode()->isStackingContext());
}

TEST_F(PaintContainmentTest, BlockPaintContainment)
{
    setBodyInnerHTML("<div id='div' style='contain: paint'></div>");
    Element* div = document().getElementById(AtomicString("div"));
    ASSERT(div);
    LayoutObject* obj = div->layoutObject();
    ASSERT(obj && obj->isLayoutBlock());
    LayoutBlock& block = toLayoutBlock(*obj);
    EXPECT_TRUE(block.createsNewFormattingContext());
    EXPECT_FALSE(block.canBeScrolledAndHasScrollableArea());
    checkIsClippingStackingContextAndContainer(block);
}

TEST_F(PaintContainmentTest, InlinePaintContainment)
{
    setBodyInnerHTML("<div><span id='test' style='contain: paint'>Foo</span></div>");
    Element* span = document().getElementById(AtomicString("test"));
    ASSERT(span);
    LayoutObject* obj = span->layoutObject();
    ASSERT(obj && obj->isInline());
    LayoutInline& layoutInline = toLayoutInline(*obj);
    checkIsClippingStackingContextAndContainer(layoutInline);
}

} // namespace blink
