// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "core/rendering/RenderMultiColumnFlowThread.h"

#include "core/rendering/RenderMultiColumnSet.h"
#include "core/rendering/RenderMultiColumnSpannerPlaceholder.h"
#include "core/rendering/RenderingTestHelper.h"

#include <gtest/gtest.h>

namespace blink {

namespace {

class MultiColumnRenderingTest : public RenderingTest {
public:
    RenderMultiColumnFlowThread* findFlowThread(const char* id) const;

    // Generate a signature string based on what kind of column boxes the flow thread has
    // established. 'c' is used for regular column content sets, while 's' is used for spanners.
    // '?' is used when there's an unknown box type (which should be considered a failure).
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
    for (RenderBox* columnBox = flowThread->firstMultiColumnBox();
        columnBox;
        columnBox = columnBox->nextSiblingMultiColumnBox()) {
        if (columnBox->isRenderMultiColumnSpannerPlaceholder())
            signature.append('s');
        else if (columnBox->isRenderMultiColumnSet())
            signature.append('c');
        else
            signature.append('?');
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
    RenderBox* columnBox = flowThread->firstMultiColumnBox();
    EXPECT_EQ(flowThread->firstMultiColumnSet(), nullptr);
    EXPECT_EQ(flowThread->containingColumnSpannerPlaceholder(document().getElementById("spanner")->renderer()), columnBox);
    EXPECT_EQ(document().getElementById("spanner")->renderer()->spannerPlaceholder(), columnBox);
}

TEST_F(MultiColumnRenderingTest, ContentThenSpanner)
{
    // With some column content followed by a spanner, we need a column set followed by a spanner set.
    setMulticolHTML("<div id='mc'><div id='columnContent'></div><div id='spanner'></div></div>");
    RenderMultiColumnFlowThread* flowThread = findFlowThread("mc");
    ASSERT_EQ(columnSetSignature(flowThread), "cs");
    RenderBox* columnBox = flowThread->lastMultiColumnBox();
    EXPECT_EQ(flowThread->containingColumnSpannerPlaceholder(document().getElementById("spanner")->renderer()), columnBox);
    EXPECT_EQ(flowThread->containingColumnSpannerPlaceholder(document().getElementById("columnContent")->renderer()), nullptr);
}

TEST_F(MultiColumnRenderingTest, SpannerThenContent)
{
    // With a spanner followed by some column content, we need a spanner set followed by a column set.
    setMulticolHTML("<div id='mc'><div id='spanner'></div><div id='columnContent'></div></div>");
    RenderMultiColumnFlowThread* flowThread = findFlowThread("mc");
    ASSERT_EQ(columnSetSignature(flowThread), "sc");
    RenderBox* columnBox = flowThread->firstMultiColumnBox();
    EXPECT_EQ(flowThread->containingColumnSpannerPlaceholder(document().getElementById("spanner")->renderer()), columnBox);
    EXPECT_EQ(flowThread->containingColumnSpannerPlaceholder(document().getElementById("columnContent")->renderer()), nullptr);
}

TEST_F(MultiColumnRenderingTest, ContentThenSpannerThenContent)
{
    // With column content followed by a spanner followed by some column content, we need a column
    // set followed by a spanner set followed by a column set.
    setMulticolHTML("<div id='mc'><div id='columnContentBefore'></div><div id='spanner'></div><div id='columnContentAfter'></div></div>");
    RenderMultiColumnFlowThread* flowThread = findFlowThread("mc");
    ASSERT_EQ(columnSetSignature(flowThread), "csc");
    RenderBox* columnBox = flowThread->firstMultiColumnSet()->nextSiblingMultiColumnBox();
    EXPECT_EQ(flowThread->containingColumnSpannerPlaceholder(document().getElementById("columnContentBefore")->renderer()), nullptr);
    EXPECT_EQ(flowThread->containingColumnSpannerPlaceholder(document().getElementById("spanner")->renderer()), columnBox);
    EXPECT_EQ(flowThread->containingColumnSpannerPlaceholder(document().getElementById("columnContentAfter")->renderer()), nullptr);
}

TEST_F(MultiColumnRenderingTest, TwoSpanners)
{
    // With two spanners and no column content, we need two spanner sets.
    setMulticolHTML("<div id='mc'><div id='spanner1'></div><div id='spanner2'></div></div>");
    RenderMultiColumnFlowThread* flowThread = findFlowThread("mc");
    ASSERT_EQ(columnSetSignature(flowThread), "ss");
    RenderBox* columnBox = flowThread->firstMultiColumnBox();
    EXPECT_EQ(flowThread->firstMultiColumnSet(), nullptr);
    EXPECT_EQ(flowThread->containingColumnSpannerPlaceholder(document().getElementById("spanner1")->renderer()), columnBox);
    EXPECT_EQ(document().getElementById("spanner1")->renderer()->spannerPlaceholder(), columnBox);
    columnBox = columnBox->nextSiblingMultiColumnBox();
    EXPECT_EQ(flowThread->containingColumnSpannerPlaceholder(document().getElementById("spanner2")->renderer()), columnBox);
    EXPECT_EQ(document().getElementById("spanner2")->renderer()->spannerPlaceholder(), columnBox);
}

TEST_F(MultiColumnRenderingTest, SpannerThenContentThenSpanner)
{
    // With two spanners and some column content in-between, we need a spanner set, a column set and another spanner set.
    setMulticolHTML("<div id='mc'><div id='spanner1'></div><div id='columnContent'></div><div id='spanner2'></div></div>");
    RenderMultiColumnFlowThread* flowThread = findFlowThread("mc");
    ASSERT_EQ(columnSetSignature(flowThread), "scs");
    RenderMultiColumnSet* columnSet = flowThread->firstMultiColumnSet();
    EXPECT_EQ(columnSet->nextSiblingMultiColumnSet(), nullptr);
    RenderBox* columnBox = flowThread->firstMultiColumnBox();
    EXPECT_EQ(flowThread->containingColumnSpannerPlaceholder(document().getElementById("spanner1")->renderer()), columnBox);
    columnBox = columnBox->nextSiblingMultiColumnBox();
    EXPECT_EQ(columnBox, columnSet);
    EXPECT_EQ(flowThread->containingColumnSpannerPlaceholder(document().getElementById("columnContent")->renderer()), nullptr);
    columnBox = columnBox->nextSiblingMultiColumnBox();
    EXPECT_EQ(flowThread->containingColumnSpannerPlaceholder(document().getElementById("spanner2")->renderer()), columnBox);
}

TEST_F(MultiColumnRenderingTest, SpannerWithSpanner)
{
    // column-span:all on something inside column-span:all has no effect.
    setMulticolHTML("<div id='mc'><div id='spanner'><div id='invalidSpanner' class='s'></div></div></div>");
    RenderMultiColumnFlowThread* flowThread = findFlowThread("mc");
    ASSERT_EQ(columnSetSignature(flowThread), "s");
    RenderBox* columnBox = flowThread->firstMultiColumnBox();
    EXPECT_EQ(flowThread->containingColumnSpannerPlaceholder(document().getElementById("spanner")->renderer()), columnBox);
    EXPECT_EQ(flowThread->containingColumnSpannerPlaceholder(document().getElementById("invalidSpanner")->renderer()), columnBox);
    EXPECT_EQ(toRenderMultiColumnSpannerPlaceholder(columnBox)->rendererInFlowThread(), document().getElementById("spanner")->renderer());
    EXPECT_EQ(document().getElementById("spanner")->renderer()->spannerPlaceholder(), columnBox);
    EXPECT_EQ(document().getElementById("invalidSpanner")->renderer()->spannerPlaceholder(), nullptr);
}

TEST_F(MultiColumnRenderingTest, SubtreeWithSpanner)
{
    setMulticolHTML("<div id='mc'><div id='outer'><div id='block1'></div><div id='spanner'></div><div id='block2'></div></div></div>");
    RenderMultiColumnFlowThread* flowThread = findFlowThread("mc");
    EXPECT_EQ(columnSetSignature(flowThread), "csc");
    RenderBox* columnBox = flowThread->firstMultiColumnSet()->nextSiblingMultiColumnBox();
    EXPECT_EQ(flowThread->containingColumnSpannerPlaceholder(document().getElementById("spanner")->renderer()), columnBox);
    EXPECT_EQ(document().getElementById("spanner")->renderer()->spannerPlaceholder(), columnBox);
    EXPECT_EQ(toRenderMultiColumnSpannerPlaceholder(columnBox)->rendererInFlowThread(), document().getElementById("spanner")->renderer());
    EXPECT_EQ(flowThread->containingColumnSpannerPlaceholder(document().getElementById("outer")->renderer()), nullptr);
    EXPECT_EQ(flowThread->containingColumnSpannerPlaceholder(document().getElementById("block1")->renderer()), nullptr);
    EXPECT_EQ(flowThread->containingColumnSpannerPlaceholder(document().getElementById("block2")->renderer()), nullptr);
}

TEST_F(MultiColumnRenderingTest, SubtreeWithSpannerAfterSpanner)
{
    setMulticolHTML("<div id='mc'><div id='spanner1'></div><div id='outer'>text<div id='spanner2'></div><div id='after'></div></div></div>");
    RenderMultiColumnFlowThread* flowThread = findFlowThread("mc");
    EXPECT_EQ(columnSetSignature(flowThread), "scsc");
    RenderBox* columnBox = flowThread->firstMultiColumnBox();
    EXPECT_EQ(flowThread->containingColumnSpannerPlaceholder(document().getElementById("spanner1")->renderer()), columnBox);
    EXPECT_EQ(toRenderMultiColumnSpannerPlaceholder(columnBox)->rendererInFlowThread(), document().getElementById("spanner1")->renderer());
    EXPECT_EQ(document().getElementById("spanner1")->renderer()->spannerPlaceholder(), columnBox);
    columnBox = columnBox->nextSiblingMultiColumnBox()->nextSiblingMultiColumnBox();
    EXPECT_EQ(flowThread->containingColumnSpannerPlaceholder(document().getElementById("spanner2")->renderer()), columnBox);
    EXPECT_EQ(toRenderMultiColumnSpannerPlaceholder(columnBox)->rendererInFlowThread(), document().getElementById("spanner2")->renderer());
    EXPECT_EQ(document().getElementById("spanner2")->renderer()->spannerPlaceholder(), columnBox);
    EXPECT_EQ(flowThread->containingColumnSpannerPlaceholder(document().getElementById("outer")->renderer()), nullptr);
    EXPECT_EQ(flowThread->containingColumnSpannerPlaceholder(document().getElementById("after")->renderer()), nullptr);
}

TEST_F(MultiColumnRenderingTest, SubtreeWithSpannerBeforeSpanner)
{
    setMulticolHTML("<div id='mc'><div id='outer'>text<div id='spanner1'></div>text</div><div id='spanner2'></div></div>");
    RenderMultiColumnFlowThread* flowThread = findFlowThread("mc");
    EXPECT_EQ(columnSetSignature(flowThread), "cscs");
    RenderBox* columnBox = flowThread->firstMultiColumnSet()->nextSiblingMultiColumnBox();
    EXPECT_EQ(flowThread->containingColumnSpannerPlaceholder(document().getElementById("spanner1")->renderer()), columnBox);
    EXPECT_EQ(document().getElementById("spanner1")->renderer()->spannerPlaceholder(), columnBox);
    EXPECT_EQ(toRenderMultiColumnSpannerPlaceholder(columnBox)->rendererInFlowThread(), document().getElementById("spanner1")->renderer());
    columnBox = columnBox->nextSiblingMultiColumnBox()->nextSiblingMultiColumnBox();
    EXPECT_EQ(flowThread->containingColumnSpannerPlaceholder(document().getElementById("spanner2")->renderer()), columnBox);
    EXPECT_EQ(document().getElementById("spanner2")->renderer()->spannerPlaceholder(), columnBox);
    EXPECT_EQ(toRenderMultiColumnSpannerPlaceholder(columnBox)->rendererInFlowThread(), document().getElementById("spanner2")->renderer());
    EXPECT_EQ(flowThread->containingColumnSpannerPlaceholder(document().getElementById("outer")->renderer()), nullptr);
}

TEST_F(MultiColumnRenderingTest, columnSetAtBlockOffset)
{
    setMulticolHTML("<div id='mc' style='line-height:100px;'>text<br>text<br>text<br>text<br>text<div id='spanner1'>spanner</div>text<br>text<div id='spanner2'>text<br>text</div>text</div>");
    RenderMultiColumnFlowThread* flowThread = findFlowThread("mc");
    EXPECT_EQ(columnSetSignature(flowThread), "cscsc");
    RenderMultiColumnSet* firstRow = flowThread->firstMultiColumnSet();
    EXPECT_EQ(flowThread->columnSetAtBlockOffset(LayoutUnit(-10000)), firstRow); // negative overflow
    EXPECT_EQ(flowThread->columnSetAtBlockOffset(LayoutUnit()), firstRow);
    EXPECT_EQ(flowThread->columnSetAtBlockOffset(LayoutUnit(499)), firstRow); // bottom of last line in first row.
    EXPECT_EQ(flowThread->columnSetAtBlockOffset(LayoutUnit(599)), firstRow); // empty content in last column in first row
    RenderMultiColumnSet* secondRow = firstRow->nextSiblingMultiColumnSet();
    EXPECT_EQ(flowThread->columnSetAtBlockOffset(LayoutUnit(600)), secondRow);
    EXPECT_EQ(flowThread->columnSetAtBlockOffset(LayoutUnit(799)), secondRow);
    RenderMultiColumnSet* thirdRow = secondRow->nextSiblingMultiColumnSet();
    EXPECT_EQ(flowThread->columnSetAtBlockOffset(LayoutUnit(800)), thirdRow);
    EXPECT_EQ(flowThread->columnSetAtBlockOffset(LayoutUnit(899)), thirdRow); // bottom of last row
    EXPECT_EQ(flowThread->columnSetAtBlockOffset(LayoutUnit(10000)), thirdRow); // overflow
}

TEST_F(MultiColumnRenderingTest, columnSetAtBlockOffsetVerticalRl)
{
    setMulticolHTML("<div id='mc' style='line-height:100px; -webkit-writing-mode:vertical-rl;'>text<br>text<br>text<br>text<br>text<div id='spanner1'>spanner</div>text<br>text<div id='spanner2'>text<br>text</div>text</div>");
    RenderMultiColumnFlowThread* flowThread = findFlowThread("mc");
    EXPECT_EQ(columnSetSignature(flowThread), "cscsc");
    RenderMultiColumnSet* firstRow = flowThread->firstMultiColumnSet();
    EXPECT_EQ(flowThread->columnSetAtBlockOffset(LayoutUnit(-10000)), firstRow); // negative overflow
    EXPECT_EQ(flowThread->columnSetAtBlockOffset(LayoutUnit()), firstRow);
    EXPECT_EQ(flowThread->columnSetAtBlockOffset(LayoutUnit(499)), firstRow); // bottom of last line in first row.
    EXPECT_EQ(flowThread->columnSetAtBlockOffset(LayoutUnit(599)), firstRow); // empty content in last column in first row
    RenderMultiColumnSet* secondRow = firstRow->nextSiblingMultiColumnSet();
    EXPECT_EQ(flowThread->columnSetAtBlockOffset(LayoutUnit(600)), secondRow);
    EXPECT_EQ(flowThread->columnSetAtBlockOffset(LayoutUnit(799)), secondRow);
    RenderMultiColumnSet* thirdRow = secondRow->nextSiblingMultiColumnSet();
    EXPECT_EQ(flowThread->columnSetAtBlockOffset(LayoutUnit(800)), thirdRow);
    EXPECT_EQ(flowThread->columnSetAtBlockOffset(LayoutUnit(899)), thirdRow); // bottom of last row
    EXPECT_EQ(flowThread->columnSetAtBlockOffset(LayoutUnit(10000)), thirdRow); // overflow
}

TEST_F(MultiColumnRenderingTest, columnSetAtBlockOffsetVerticalLr)
{
    setMulticolHTML("<div id='mc' style='line-height:100px; -webkit-writing-mode:vertical-lr;'>text<br>text<br>text<br>text<br>text<div id='spanner1'>spanner</div>text<br>text<div id='spanner2'>text<br>text</div>text</div>");
    RenderMultiColumnFlowThread* flowThread = findFlowThread("mc");
    EXPECT_EQ(columnSetSignature(flowThread), "cscsc");
    RenderMultiColumnSet* firstRow = flowThread->firstMultiColumnSet();
    EXPECT_EQ(flowThread->columnSetAtBlockOffset(LayoutUnit(-10000)), firstRow); // negative overflow
    EXPECT_EQ(flowThread->columnSetAtBlockOffset(LayoutUnit()), firstRow);
    EXPECT_EQ(flowThread->columnSetAtBlockOffset(LayoutUnit(499)), firstRow); // bottom of last line in first row.
    EXPECT_EQ(flowThread->columnSetAtBlockOffset(LayoutUnit(599)), firstRow); // empty content in last column in first row
    RenderMultiColumnSet* secondRow = firstRow->nextSiblingMultiColumnSet();
    EXPECT_EQ(flowThread->columnSetAtBlockOffset(LayoutUnit(600)), secondRow);
    EXPECT_EQ(flowThread->columnSetAtBlockOffset(LayoutUnit(799)), secondRow);
    RenderMultiColumnSet* thirdRow = secondRow->nextSiblingMultiColumnSet();
    EXPECT_EQ(flowThread->columnSetAtBlockOffset(LayoutUnit(800)), thirdRow);
    EXPECT_EQ(flowThread->columnSetAtBlockOffset(LayoutUnit(899)), thirdRow); // bottom of last row
    EXPECT_EQ(flowThread->columnSetAtBlockOffset(LayoutUnit(10000)), thirdRow); // overflow
}

} // anonymous namespace

} // namespace blink
