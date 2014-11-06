// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "core/rendering/RenderMultiColumnFlowThread.h"

#include "core/rendering/RenderMultiColumnSet.h"
#include "core/rendering/RenderMultiColumnSpannerSet.h"
#include "core/rendering/RenderingTestHelper.h"

#include <gtest/gtest.h>

namespace blink {

namespace {

class MultiColumnRenderingTest : public RenderingTest {
public:
    RenderMultiColumnFlowThread* findFlowThread(const char* id) const;

    // Generate a signature string based on what kind of column sets the flow thread has
    // established. There will be one character for each column set. 'c' is used for regular column
    // content sets, while 's' is used for spanner sets.
    String columnSetSignature(RenderMultiColumnFlowThread*);
    String columnSetSignature(const char* multicolId);

    void setMulticolHTML(const String&);
};

RenderMultiColumnFlowThread* MultiColumnRenderingTest::findFlowThread(const char* id) const
{
    Node* multicol = document().getElementById(id);
    if (!multicol)
        return 0;
    RenderBlockFlow* multicolContainer = toRenderBlockFlow(multicol->renderer());
    if (!multicolContainer)
        return 0;
    return multicolContainer->multiColumnFlowThread();
}

String MultiColumnRenderingTest::columnSetSignature(RenderMultiColumnFlowThread* flowThread)
{
    String signature = "";
    for (RenderMultiColumnSet* columnSet = flowThread->firstMultiColumnSet(); columnSet; columnSet = columnSet->nextSiblingMultiColumnSet()) {
        if (columnSet->isRenderMultiColumnSpannerSet())
            signature.append('s');
        else
            signature.append('c');
    }
    return signature;
}

String MultiColumnRenderingTest::columnSetSignature(const char* multicolId)
{
    return columnSetSignature(findFlowThread(multicolId));
}

void MultiColumnRenderingTest::setMulticolHTML(const String& html)
{
    const char* style =
        "<style>"
        "  #mc { -webkit-columns:2; }"
        "  .s, #spanner, #spanner1, #spanner2 { -webkit-column-span:all; }"
        "</style>";
    setBodyInnerHTML(style + html);
}

TEST_F(MultiColumnRenderingTest, OneBlockWithInDepthTreeStructureCheck)
{
    // Examine the render tree established by a simple multicol container with a block with some text inside.
    setMulticolHTML("<div id='mc'><div>xxx</div></div>");
    Node* multicol = document().getElementById("mc");
    ASSERT_TRUE(multicol);
    RenderBlockFlow* multicolContainer = toRenderBlockFlow(multicol->renderer());
    ASSERT_TRUE(multicolContainer);
    RenderMultiColumnFlowThread* flowThread = multicolContainer->multiColumnFlowThread();
    ASSERT_TRUE(flowThread);
    EXPECT_EQ(columnSetSignature(flowThread), "c");
    EXPECT_EQ(flowThread->parent(), multicolContainer);
    EXPECT_FALSE(flowThread->previousSibling());
    RenderMultiColumnSet* columnSet = flowThread->firstMultiColumnSet();
    ASSERT_TRUE(columnSet);
    EXPECT_EQ(columnSet->previousSibling(), flowThread);
    EXPECT_FALSE(columnSet->nextSibling());
    RenderBlockFlow* block = toRenderBlockFlow(flowThread->firstChild());
    ASSERT_TRUE(block);
    EXPECT_FALSE(block->nextSibling());
    ASSERT_TRUE(block->firstChild());
    EXPECT_TRUE(block->firstChild()->isText());
    EXPECT_FALSE(block->firstChild()->nextSibling());
}

TEST_F(MultiColumnRenderingTest, Empty)
{
    // If there's no column content, there should be no column set.
    setMulticolHTML("<div id='mc'></div>");
    EXPECT_EQ(columnSetSignature("mc"), "");
}

TEST_F(MultiColumnRenderingTest, OneBlock)
{
    // There is some content, so we should create a column set.
    setMulticolHTML("<div id='mc'><div id='block'></div></div>");
    ASSERT_EQ(columnSetSignature("mc"), "c");
}

TEST_F(MultiColumnRenderingTest, TwoBlocks)
{
    // No matter how much content, we should only create one column set (unless there are spanners).
    setMulticolHTML("<div id='mc'><div id='block1'></div><div id='block2'></div></div>");
    ASSERT_EQ(columnSetSignature("mc"), "c");
}

TEST_F(MultiColumnRenderingTest, Spanner)
{
    // With one spanner and no column content, we should create a spanner set.
    setMulticolHTML("<div id='mc'><div id='spanner'></div></div>");
    RenderMultiColumnFlowThread* flowThread = findFlowThread("mc");
    ASSERT_EQ(columnSetSignature(flowThread), "s");
    RenderMultiColumnSet* columnSet = flowThread->firstMultiColumnSet();
    EXPECT_EQ(flowThread->containingColumnSpannerSet(document().getElementById("spanner")->renderer()), columnSet);
}

TEST_F(MultiColumnRenderingTest, ContentThenSpanner)
{
    // With some column content followed by a spanner, we need a column set followed by a spanner set.
    setMulticolHTML("<div id='mc'><div id='columnContent'></div><div id='spanner'></div></div>");
    RenderMultiColumnFlowThread* flowThread = findFlowThread("mc");
    ASSERT_EQ(columnSetSignature(flowThread), "cs");
    RenderMultiColumnSet* columnSet = flowThread->lastMultiColumnSet();
    EXPECT_EQ(flowThread->containingColumnSpannerSet(document().getElementById("spanner")->renderer()), columnSet);
    EXPECT_EQ(flowThread->containingColumnSpannerSet(document().getElementById("columnContent")->renderer()), nullptr);
}

TEST_F(MultiColumnRenderingTest, SpannerThenContent)
{
    // With a spanner followed by some column content, we need a spanner set followed by a column set.
    setMulticolHTML("<div id='mc'><div id='spanner'></div><div id='columnContent'></div></div>");
    RenderMultiColumnFlowThread* flowThread = findFlowThread("mc");
    ASSERT_EQ(columnSetSignature(flowThread), "sc");
    RenderMultiColumnSet* columnSet = flowThread->firstMultiColumnSet();
    EXPECT_EQ(flowThread->containingColumnSpannerSet(document().getElementById("spanner")->renderer()), columnSet);
    EXPECT_EQ(flowThread->containingColumnSpannerSet(document().getElementById("columnContent")->renderer()), nullptr);
}

TEST_F(MultiColumnRenderingTest, ContentThenSpannerThenContent)
{
    // With column content followed by a spanner followed by some column content, we need a column
    // set followed by a spanner set followed by a column set.
    setMulticolHTML("<div id='mc'><div id='columnContentBefore'></div><div id='spanner'></div><div id='columnContentAfter'></div></div>");
    RenderMultiColumnFlowThread* flowThread = findFlowThread("mc");
    ASSERT_EQ(columnSetSignature(flowThread), "csc");
    RenderMultiColumnSet* columnSet = flowThread->firstMultiColumnSet()->nextSiblingMultiColumnSet();
    EXPECT_EQ(flowThread->containingColumnSpannerSet(document().getElementById("columnContentBefore")->renderer()), nullptr);
    EXPECT_EQ(flowThread->containingColumnSpannerSet(document().getElementById("spanner")->renderer()), columnSet);
    EXPECT_EQ(flowThread->containingColumnSpannerSet(document().getElementById("columnContentAfter")->renderer()), nullptr);
}

TEST_F(MultiColumnRenderingTest, TwoSpanners)
{
    // With two spanners and no column content, we need two spanner sets.
    setMulticolHTML("<div id='mc'><div id='spanner1'></div><div id='spanner2'></div></div>");
    RenderMultiColumnFlowThread* flowThread = findFlowThread("mc");
    ASSERT_EQ(columnSetSignature(flowThread), "ss");
    RenderMultiColumnSet* columnSet = flowThread->firstMultiColumnSet();
    EXPECT_EQ(flowThread->containingColumnSpannerSet(document().getElementById("spanner1")->renderer()), columnSet);
    columnSet = columnSet->nextSiblingMultiColumnSet();
    EXPECT_EQ(flowThread->containingColumnSpannerSet(document().getElementById("spanner2")->renderer()), columnSet);
}

TEST_F(MultiColumnRenderingTest, SpannerThenContentThenSpanner)
{
    // With two spanners and some column content in-between, we need a spanner set, a column set and another spanner set.
    setMulticolHTML("<div id='mc'><div id='spanner1'></div><div id='columnContent'></div><div id='spanner2'></div></div>");
    RenderMultiColumnFlowThread* flowThread = findFlowThread("mc");
    ASSERT_EQ(columnSetSignature(flowThread), "scs");
    RenderMultiColumnSet* columnSet = flowThread->firstMultiColumnSet();
    EXPECT_EQ(flowThread->containingColumnSpannerSet(document().getElementById("spanner1")->renderer()), columnSet);
    EXPECT_EQ(flowThread->containingColumnSpannerSet(document().getElementById("columnContent")->renderer()), nullptr);
    columnSet = columnSet->nextSiblingMultiColumnSet()->nextSiblingMultiColumnSet();
    EXPECT_EQ(flowThread->containingColumnSpannerSet(document().getElementById("spanner2")->renderer()), columnSet);
}

TEST_F(MultiColumnRenderingTest, SpannerWithSpanner)
{
    // column-span:all on something inside column-span:all has no effect.
    setMulticolHTML("<div id='mc'><div id='spanner'><div id='invalidSpanner' class='s'></div></div></div>");
    RenderMultiColumnFlowThread* flowThread = findFlowThread("mc");
    ASSERT_EQ(columnSetSignature(flowThread), "s");
    RenderMultiColumnSet* columnSet = flowThread->firstMultiColumnSet();
    EXPECT_EQ(flowThread->containingColumnSpannerSet(document().getElementById("spanner")->renderer()), columnSet);
    EXPECT_EQ(flowThread->containingColumnSpannerSet(document().getElementById("invalidSpanner")->renderer()), columnSet);
    EXPECT_EQ(toRenderMultiColumnSpannerSet(columnSet)->rendererInFlowThread(), document().getElementById("spanner")->renderer());
}

TEST_F(MultiColumnRenderingTest, SubtreeWithSpanner)
{
    setMulticolHTML("<div id='mc'><div id='outer'><div id='block1'></div><div id='spanner'></div><div id='block2'></div></div></div>");
    RenderMultiColumnFlowThread* flowThread = findFlowThread("mc");
    EXPECT_EQ(columnSetSignature(flowThread), "csc");
    RenderMultiColumnSet* columnSet = flowThread->firstMultiColumnSet()->nextSiblingMultiColumnSet();
    EXPECT_EQ(flowThread->containingColumnSpannerSet(document().getElementById("spanner")->renderer()), columnSet);
    EXPECT_EQ(toRenderMultiColumnSpannerSet(columnSet)->rendererInFlowThread(), document().getElementById("spanner")->renderer());
    EXPECT_EQ(flowThread->containingColumnSpannerSet(document().getElementById("outer")->renderer()), nullptr);
    EXPECT_EQ(flowThread->containingColumnSpannerSet(document().getElementById("block1")->renderer()), nullptr);
    EXPECT_EQ(flowThread->containingColumnSpannerSet(document().getElementById("block2")->renderer()), nullptr);
}

TEST_F(MultiColumnRenderingTest, SubtreeWithSpannerAfterSpanner)
{
    setMulticolHTML("<div id='mc'><div id='spanner1'></div><div id='outer'>text<div id='spanner2'></div><div id='after'></div></div></div>");
    RenderMultiColumnFlowThread* flowThread = findFlowThread("mc");
    EXPECT_EQ(columnSetSignature(flowThread), "scsc");
    RenderMultiColumnSet* columnSet = flowThread->firstMultiColumnSet();
    EXPECT_EQ(flowThread->containingColumnSpannerSet(document().getElementById("spanner1")->renderer()), columnSet);
    EXPECT_EQ(toRenderMultiColumnSpannerSet(columnSet)->rendererInFlowThread(), document().getElementById("spanner1")->renderer());
    columnSet = columnSet->nextSiblingMultiColumnSet()->nextSiblingMultiColumnSet();
    EXPECT_EQ(flowThread->containingColumnSpannerSet(document().getElementById("spanner2")->renderer()), columnSet);
    EXPECT_EQ(toRenderMultiColumnSpannerSet(columnSet)->rendererInFlowThread(), document().getElementById("spanner2")->renderer());
    EXPECT_EQ(flowThread->containingColumnSpannerSet(document().getElementById("outer")->renderer()), nullptr);
    EXPECT_EQ(flowThread->containingColumnSpannerSet(document().getElementById("after")->renderer()), nullptr);
}

TEST_F(MultiColumnRenderingTest, SubtreeWithSpannerBeforeSpanner)
{
    setMulticolHTML("<div id='mc'><div id='outer'>text<div id='spanner1'></div>text</div><div id='spanner2'></div></div>");
    RenderMultiColumnFlowThread* flowThread = findFlowThread("mc");
    EXPECT_EQ(columnSetSignature(flowThread), "cscs");
    RenderMultiColumnSet* columnSet = flowThread->firstMultiColumnSet()->nextSiblingMultiColumnSet();
    EXPECT_EQ(flowThread->containingColumnSpannerSet(document().getElementById("spanner1")->renderer()), columnSet);
    EXPECT_EQ(toRenderMultiColumnSpannerSet(columnSet)->rendererInFlowThread(), document().getElementById("spanner1")->renderer());
    columnSet = columnSet->nextSiblingMultiColumnSet()->nextSiblingMultiColumnSet();
    EXPECT_EQ(flowThread->containingColumnSpannerSet(document().getElementById("spanner2")->renderer()), columnSet);
    EXPECT_EQ(toRenderMultiColumnSpannerSet(columnSet)->rendererInFlowThread(), document().getElementById("spanner2")->renderer());
    EXPECT_EQ(flowThread->containingColumnSpannerSet(document().getElementById("outer")->renderer()), nullptr);
}

} // anonymous namespace

} // namespace blink
