// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_base_layout_algorithm_test.h"

#include <sstream>
#include "third_party/blink/renderer/core/dom/tag_collection.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_box_state.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_child_layout_context.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

namespace blink {
namespace {

const NGPhysicalLineBoxFragment* FindBlockInInlineLineBoxFragment(
    Element* container) {
  NGInlineCursor cursor(*To<LayoutBlockFlow>(container->GetLayoutObject()));
  for (cursor.MoveToFirstLine(); cursor; cursor.MoveToNextLine()) {
    const NGPhysicalLineBoxFragment* fragment =
        cursor.Current()->LineBoxFragment();
    DCHECK(fragment);
    if (fragment->IsBlockInInline())
      return fragment;
  }
  return nullptr;
}

class NGInlineLayoutAlgorithmTest : public NGBaseLayoutAlgorithmTest {
 protected:
  static std::string AsFragmentItemsString(const LayoutBlockFlow& root) {
    std::ostringstream ostream;
    ostream << std::endl;
    for (NGInlineCursor cursor(root); cursor; cursor.MoveToNext()) {
      const auto& item = *cursor.CurrentItem();
      ostream << item << " " << item.RectInContainerFragment() << std::endl;
    }
    return ostream.str();
  }
};

TEST_F(NGInlineLayoutAlgorithmTest, Types) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <div id="normal">normal</div>
    <div id="empty"><span></span></div>
  )HTML");
  NGInlineCursor normal(
      *To<LayoutBlockFlow>(GetLayoutObjectByElementId("normal")));
  normal.MoveToFirstLine();
  EXPECT_FALSE(normal.Current()->LineBoxFragment()->IsEmptyLineBox());

  NGInlineCursor empty(
      *To<LayoutBlockFlow>(GetLayoutObjectByElementId("empty")));
  empty.MoveToFirstLine();
  EXPECT_TRUE(empty.Current()->LineBoxFragment()->IsEmptyLineBox());
}

TEST_F(NGInlineLayoutAlgorithmTest, TypesForBlockInInline) {
  ScopedLayoutNGBlockInInlineForTest block_in_inline_scope(true);
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <div id="block-in-inline">
      <span><div>normal</div></span>
    </div>
    <div id="block-in-inline-empty">
      <span><div></div></span>
    </div>
    <div id="block-in-inline-height">
      <span><div style="height: 100px"></div></span>
    </div>
  )HTML");
  // Regular block-in-inline.
  NGInlineCursor block_in_inline(
      *To<LayoutBlockFlow>(GetLayoutObjectByElementId("block-in-inline")));
  block_in_inline.MoveToFirstLine();
  EXPECT_TRUE(block_in_inline.Current()->LineBoxFragment()->IsEmptyLineBox());
  EXPECT_FALSE(block_in_inline.Current()->LineBoxFragment()->IsBlockInInline());
  block_in_inline.MoveToNextLine();
  EXPECT_FALSE(block_in_inline.Current()->LineBoxFragment()->IsEmptyLineBox());
  EXPECT_TRUE(block_in_inline.Current()->LineBoxFragment()->IsBlockInInline());
  int block_count = 0;
  for (NGInlineCursor children = block_in_inline.CursorForDescendants();
       children; children.MoveToNext()) {
    if (children.Current()->BoxFragment() &&
        children.Current()->BoxFragment()->IsBlockInInline())
      ++block_count;
  }
  EXPECT_EQ(block_count, 1);
  block_in_inline.MoveToNextLine();
  EXPECT_TRUE(block_in_inline.Current()->LineBoxFragment()->IsEmptyLineBox());
  EXPECT_FALSE(block_in_inline.Current()->LineBoxFragment()->IsBlockInInline());

  // If the block is empty and self-collapsing, |IsEmptyLineBox| should be set.
  NGInlineCursor block_in_inline_empty(*To<LayoutBlockFlow>(
      GetLayoutObjectByElementId("block-in-inline-empty")));
  block_in_inline_empty.MoveToFirstLine();
  block_in_inline_empty.MoveToNextLine();
  EXPECT_TRUE(
      block_in_inline_empty.Current()->LineBoxFragment()->IsEmptyLineBox());
  EXPECT_TRUE(
      block_in_inline_empty.Current()->LineBoxFragment()->IsBlockInInline());

  // Test empty but non-self-collapsing block in an inline box.
  NGInlineCursor block_in_inline_height(*To<LayoutBlockFlow>(
      GetLayoutObjectByElementId("block-in-inline-height")));
  block_in_inline_height.MoveToFirstLine();
  block_in_inline_height.MoveToNextLine();
  EXPECT_FALSE(
      block_in_inline_height.Current()->LineBoxFragment()->IsEmptyLineBox());
  EXPECT_TRUE(
      block_in_inline_height.Current()->LineBoxFragment()->IsBlockInInline());
}

TEST_F(NGInlineLayoutAlgorithmTest, BreakToken) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      html {
        font: 10px/1 Ahem;
      }
      #container {
        width: 50px; height: 20px;
      }
    </style>
    <div id=container>123 456 789</div>
  )HTML");

  // Perform 1st Layout.
  auto* block_flow =
      To<LayoutBlockFlow>(GetLayoutObjectByElementId("container"));
  NGInlineNode inline_node(block_flow);
  LogicalSize size(LayoutUnit(50), LayoutUnit(20));

  NGConstraintSpaceBuilder builder(
      WritingMode::kHorizontalTb,
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      /* is_new_fc */ false);
  builder.SetAvailableSize(size);
  NGConstraintSpace constraint_space = builder.ToConstraintSpace();

  NGInlineChildLayoutContext context;
  NGBoxFragmentBuilder container_builder(
      block_flow, block_flow->Style(),
      block_flow->Style()->GetWritingDirection());
  NGFragmentItemsBuilder items_builder(inline_node,
                                       container_builder.GetWritingDirection());
  container_builder.SetItemsBuilder(&items_builder);
  context.SetItemsBuilder(&items_builder);
  const NGLayoutResult* layout_result =
      inline_node.Layout(constraint_space, nullptr, &context);
  const auto& line1 = layout_result->PhysicalFragment();
  EXPECT_TRUE(line1.BreakToken());

  // Perform 2nd layout with the break token from the 1st line.
  const NGLayoutResult* layout_result2 =
      inline_node.Layout(constraint_space, line1.BreakToken(), &context);
  const auto& line2 = layout_result2->PhysicalFragment();
  EXPECT_TRUE(line2.BreakToken());

  // Perform 3rd layout with the break token from the 2nd line.
  const NGLayoutResult* layout_result3 =
      inline_node.Layout(constraint_space, line2.BreakToken(), &context);
  const auto& line3 = layout_result3->PhysicalFragment();
  EXPECT_FALSE(line3.BreakToken());
}

// This test ensures box fragments are generated when necessary, even when the
// line is empty. One such case is when the line contains a containing box of an
// out-of-flow object.
TEST_F(NGInlineLayoutAlgorithmTest,
       EmptyLineWithOutOfFlowInInlineContainingBlock) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    oof-container {
      position: relative;
    }
    oof {
      position: absolute;
      width: 100px;
      height: 100px;
    }
    html, body { margin: 0; }
    html {
      font-size: 10px;
    }
    </style>
    <div id=container>
      <oof-container id=target>
        <oof></oof>
      </oof-container>
    </div>
  )HTML");
  auto* block_flow =
      To<LayoutBlockFlow>(GetLayoutObjectByElementId("container"));
  const NGPhysicalBoxFragment* container = block_flow->GetPhysicalFragment(0);
  ASSERT_TRUE(container);
  EXPECT_EQ(LayoutUnit(), container->Size().height);

  NGInlineCursor line_box(*block_flow);
  ASSERT_TRUE(line_box);
  ASSERT_TRUE(line_box.Current().IsLineBox());
  EXPECT_EQ(PhysicalSize(), line_box.Current().Size());

  NGInlineCursor off_container(line_box);
  off_container.MoveToNext();
  ASSERT_TRUE(off_container);
  ASSERT_EQ(GetLayoutObjectByElementId("target"),
            off_container.Current().GetLayoutObject());
  EXPECT_EQ(PhysicalSize(), off_container.Current().Size());
}

// This test ensures that if an inline box generates (or does not generate) box
// fragments for a wrapped line, it should consistently do so for other lines
// too, when the inline box is fragmented to multiple lines.
TEST_F(NGInlineLayoutAlgorithmTest, BoxForEndMargin) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    html, body { margin: 0; }
    #container {
      font: 10px/1 Ahem;
      width: 50px;
    }
    span {
      border-right: 10px solid blue;
    }
    </style>
    <!-- This line wraps, and only 2nd line has a border. -->
    <div id=container>12 <span id=span>3 45</span> 6</div>
  )HTML");
  auto* block_flow =
      To<LayoutBlockFlow>(GetLayoutObjectByElementId("container"));
  NGInlineCursor line_box(*block_flow);
  ASSERT_TRUE(line_box) << "line_box is at start of first line.";
  ASSERT_TRUE(line_box.Current().IsLineBox());
  line_box.MoveToNextLine();
  ASSERT_TRUE(line_box) << "line_box is at start of second line.";
  NGInlineCursor cursor(line_box);
  ASSERT_TRUE(line_box.Current().IsLineBox());
  cursor.MoveToNext();
  ASSERT_TRUE(cursor);
  EXPECT_EQ(GetLayoutObjectByElementId("span"),
            cursor.Current().GetLayoutObject());

  // The <span> generates a box fragment for the 2nd line because it has a
  // right border. It should also generate a box fragment for the 1st line even
  // though there's no borders on the 1st line.
  const NGPhysicalBoxFragment* box_fragment = cursor.Current().BoxFragment();
  ASSERT_TRUE(box_fragment);
  EXPECT_EQ(NGPhysicalFragment::kFragmentBox, box_fragment->Type());

  line_box.MoveToNextLine();
  ASSERT_FALSE(line_box) << "block_flow has two lines.";
}

// A block with inline children generates fragment tree as follows:
// - A box fragment created by NGBlockNode
//   - A wrapper box fragment created by NGInlineNode
//     - Line box fragments.
// This test verifies that borders/paddings are applied to the wrapper box.
TEST_F(NGInlineLayoutAlgorithmTest, ContainerBorderPadding) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    html, body { margin: 0; }
    div {
      padding-left: 5px;
      padding-top: 10px;
      display: flow-root;
    }
    </style>
    <div id=container>test</div>
  )HTML");
  auto* block_flow =
      To<LayoutBlockFlow>(GetLayoutObjectByElementId("container"));
  NGBlockNode block_node(block_flow);
  NGConstraintSpace space =
      NGConstraintSpace::CreateFromLayoutObject(*block_flow);
  const NGLayoutResult* layout_result = block_node.Layout(space);

  EXPECT_TRUE(layout_result->BfcBlockOffset().has_value());
  EXPECT_EQ(0, *layout_result->BfcBlockOffset());
  EXPECT_EQ(0, layout_result->BfcLineOffset());

  PhysicalOffset line_offset =
      layout_result->PhysicalFragment().Children()[0].Offset();
  EXPECT_EQ(5, line_offset.left);
  EXPECT_EQ(10, line_offset.top);
}

// The test leaks memory. crbug.com/721932
#if defined(ADDRESS_SANITIZER)
#define MAYBE_VerticalAlignBottomReplaced DISABLED_VerticalAlignBottomReplaced
#else
#define MAYBE_VerticalAlignBottomReplaced VerticalAlignBottomReplaced
#endif
TEST_F(NGInlineLayoutAlgorithmTest, MAYBE_VerticalAlignBottomReplaced) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    html { font-size: 10px; }
    img { vertical-align: bottom; }
    #container { display: flow-root; }
    </style>
    <div id=container><img src="#" width="96" height="96"></div>
  )HTML");
  auto* block_flow =
      To<LayoutBlockFlow>(GetLayoutObjectByElementId("container"));
  NGInlineCursor cursor(*block_flow);
  ASSERT_TRUE(cursor);
  EXPECT_EQ(LayoutUnit(96), cursor.Current().Size().height);
  cursor.MoveToNext();
  ASSERT_TRUE(cursor);
  EXPECT_EQ(LayoutUnit(0), cursor.Current().OffsetInContainerFragment().top)
      << "Offset top of <img> should be zero.";
}

// Verifies that text can flow correctly around floats that were positioned
// before the inline block.
TEST_F(NGInlineLayoutAlgorithmTest, TextFloatsAroundFloatsBefore) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      * {
        font-family: "Arial", sans-serif;
        font-size: 20px;
      }
      #container {
        height: 200px; width: 200px; outline: solid blue;
      }
      #left-float1 {
        float: left; width: 30px; height: 30px; background-color: blue;
      }
      #left-float2 {
        float: left; width: 10px; height: 10px;
        background-color: purple;
      }
      #right-float {
        float: right; width: 40px; height: 40px; background-color: yellow;
      }
    </style>
    <div id="container">
      <div id="left-float1"></div>
      <div id="left-float2"></div>
      <div id="right-float"></div>
      <span id="text">The quick brown fox jumps over the lazy dog</span>
    </div>
  )HTML");
  // ** Run LayoutNG algorithm **
  auto [html_fragment, space] = RunBlockLayoutAlgorithmForElement(
      GetDocument().getElementsByTagName("html")->item(0));
  auto* body_fragment =
      To<NGPhysicalBoxFragment>(html_fragment->Children()[0].get());
  auto* container_fragment =
      To<NGPhysicalBoxFragment>(body_fragment->Children()[0].get());
  Vector<PhysicalOffset> line_offsets;
  for (const auto& child : container_fragment->Children()) {
    if (!child->IsLineBox())
      continue;

    line_offsets.push_back(child.Offset());
  }

  // Line break points may vary by minor differences in fonts.
  // The test is valid as long as we have 3 or more lines and their positions
  // are correct.
  EXPECT_GE(line_offsets.size(), 3UL);

  // 40 = #left-float1' width 30 + #left-float2 10
  EXPECT_EQ(LayoutUnit(40), line_offsets[0].left);

  // 40 = #left-float1' width 30
  EXPECT_EQ(LayoutUnit(30), line_offsets[1].left);
  EXPECT_EQ(LayoutUnit(), line_offsets[2].left);
}

// Verifies that text correctly flows around the inline float that fits on
// the same text line.
TEST_F(NGInlineLayoutAlgorithmTest, TextFloatsAroundInlineFloatThatFitsOnLine) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      * {
        font-family: "Arial", sans-serif;
        font-size: 18px;
      }
      #container {
        height: 200px; width: 200px; outline: solid orange;
      }
      #narrow-float {
        float: left; width: 30px; height: 30px; background-color: blue;
      }
    </style>
    <div id="container">
      <span id="text">
        The quick <div id="narrow-float"></div> brown fox jumps over the lazy
      </span>
    </div>
  )HTML");

  auto* block_flow =
      To<LayoutBlockFlow>(GetLayoutObjectByElementId("container"));
  const NGPhysicalBoxFragment* block_box = block_flow->GetPhysicalFragment(0);
  ASSERT_TRUE(block_box);

  // Two lines.
  ASSERT_EQ(2u, block_box->Children().size());
  PhysicalOffset first_line_offset = block_box->Children()[1].Offset();

  // 30 == narrow-float's width.
  EXPECT_EQ(LayoutUnit(30), first_line_offset.left);

  Element* span = GetDocument().getElementById("text");
  // 38 == narrow-float's width + body's margin.
  EXPECT_EQ(LayoutUnit(38), span->OffsetLeft());

  Element* narrow_float = GetDocument().getElementById("narrow-float");
  // 8 == body's margin.
  EXPECT_EQ(8, narrow_float->OffsetLeft());
  EXPECT_EQ(8, narrow_float->OffsetTop());
}

// Verifies that the inline float got pushed to the next line if it doesn't
// fit the current line.
TEST_F(NGInlineLayoutAlgorithmTest,
       TextFloatsAroundInlineFloatThatDoesNotFitOnLine) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      * {
        font-family: "Arial", sans-serif;
        font-size: 19px;
      }
      #container {
        height: 200px; width: 200px; outline: solid orange;
      }
      #wide-float {
        float: left; width: 160px; height: 30px; background-color: red;
      }
    </style>
    <div id="container">
      <span id="text">
        The quick <div id="wide-float"></div> brown fox jumps over the lazy dog
      </span>
    </div>
  )HTML");

  Element* wide_float = GetDocument().getElementById("wide-float");
  // 8 == body's margin.
  EXPECT_EQ(8, wide_float->OffsetLeft());
}

// Verifies that if an inline float pushed to the next line then all others
// following inline floats positioned with respect to the float's top edge
// alignment rule.
TEST_F(NGInlineLayoutAlgorithmTest,
       FloatsArePositionedWithRespectToTopEdgeAlignmentRule) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      * {
        font-family: "Arial", sans-serif;
        font-size: 19px;
      }
      #container {
        height: 200px; width: 200px; outline: solid orange;
      }
      #left-narrow {
        float: left; width: 5px; height: 30px; background-color: blue;
      }
      #left-wide {
        float: left; width: 160px; height: 30px; background-color: red;
      }
    </style>
    <div id="container">
      <span id="text">
        The quick <div id="left-wide"></div> brown <div id="left-narrow"></div>
        fox jumps over the lazy dog
      </span>
    </div>
  )HTML");
  Element* wide_float = GetDocument().getElementById("left-wide");
  // 8 == body's margin.
  EXPECT_EQ(8, wide_float->OffsetLeft());

  Element* narrow_float = GetDocument().getElementById("left-narrow");
  // 160 float-wide's width + 8 body's margin.
  EXPECT_EQ(160 + 8, narrow_float->OffsetLeft());

  // On the same line.
  EXPECT_EQ(wide_float->OffsetTop(), narrow_float->OffsetTop());
}

// Block-in-inline is not reusable. See |EndOfReusableItems|.
TEST_F(NGInlineLayoutAlgorithmTest, BlockInInlineAppend) {
  ScopedLayoutNGBlockInInlineForTest scoped_for_test(true);
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      :root {
        font-size: 10px;
      }
      #container {
        width: 10ch;
      }
    </style>
    <div id="container">
      <span id="span">
        12345678
        <div>block</div>
        12345678
      </span>
      12345678
    </div>
  )HTML");
  Element* container_element = GetElementById("container");
  scoped_refptr<const NGPhysicalLineBoxFragment> before_append =
      FindBlockInInlineLineBoxFragment(container_element);
  ASSERT_TRUE(before_append);

  Document& doc = GetDocument();
  container_element->appendChild(doc.createTextNode("12345678"));
  UpdateAllLifecyclePhasesForTest();
  scoped_refptr<const NGPhysicalLineBoxFragment> after_append =
      FindBlockInInlineLineBoxFragment(container_element);
  EXPECT_NE(before_append, after_append);
}

// Verifies that InlineLayoutAlgorithm positions floats with respect to their
// margins.
TEST_F(NGInlineLayoutAlgorithmTest, PositionFloatsWithMargins) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      #container {
        height: 200px; width: 200px; outline: solid orange;
      }
      #left {
        float: left; width: 5px; height: 30px; background-color: blue;
        margin: 10%;
      }
    </style>
    <div id="container">
      <span id="text">
        The quick <div id="left"></div> brown fox jumps over the lazy dog
      </span>
    </div>
  )HTML");
  Element* span = GetElementById("text");
  // 53 = sum of left's inline margins: 40 + left's width: 5 + body's margin: 8
  EXPECT_EQ(LayoutUnit(53), span->OffsetLeft());
}

// Test glyph bounding box causes ink overflow.
TEST_F(NGInlineLayoutAlgorithmTest, InkOverflow) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      #container {
        font: 20px/.5 Ahem;
        display: flow-root;
      }
    </style>
    <div id="container">Hello</div>
  )HTML");
  auto* block_flow =
      To<LayoutBlockFlow>(GetLayoutObjectByElementId("container"));
  const NGPhysicalBoxFragment& box_fragment =
      *block_flow->GetPhysicalFragment(0);
  EXPECT_EQ(LayoutUnit(10), box_fragment.Size().height);

  NGInlineCursor cursor(*block_flow);
  PhysicalRect ink_overflow = cursor.Current().InkOverflow();
  EXPECT_EQ(LayoutUnit(-5), ink_overflow.offset.top);
  EXPECT_EQ(LayoutUnit(20), ink_overflow.size.height);
}

// See also NGInlineLayoutAlgorithmTest.TextCombineFake
TEST_F(NGInlineLayoutAlgorithmTest, TextCombineBasic) {
  ScopedLayoutNGTextCombineForTest enable_layout_ng_text_combine(true);
  LoadAhem();
  InsertStyleElement(
      "body { margin: 0px; font: 100px/110px Ahem; }"
      "c { text-combine-upright: all; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root>a<c id=target>01234</c>b</div>");

  EXPECT_EQ(R"DUMP(
{Line #descendants=5 LTR Standard} "0,0 110x300"
{Text 0-1 LTR Standard} "5,0 100x100"
{Box #descendants=2 Standard} "5,100 100x100"
{Box #descendants=1 AtomicInlineLTR Standard} "5,100 100x100"
{Text 2-3 LTR Standard} "5,200 100x100"
)DUMP",
            AsFragmentItemsString(
                *To<LayoutBlockFlow>(GetLayoutObjectByElementId("root"))));

  EXPECT_EQ(R"DUMP(
{Line #descendants=2 LTR Standard} "0,0 100x100"
{Text 0-5 LTR Standard} "0,0 500x100"
)DUMP",
            AsFragmentItemsString(*To<LayoutBlockFlow>(
                GetLayoutObjectByElementId("target")->SlowFirstChild())));
}

// See also NGInlineLayoutAlgorithmTest.TextCombineBasic
TEST_F(NGInlineLayoutAlgorithmTest, TextCombineFake) {
  ScopedLayoutNGTextCombineForTest enable_layout_ng_text_combine(true);
  LoadAhem();
  InsertStyleElement(
      "body { margin: 0px; font: 100px/110px Ahem; }"
      "c {"
      "  display: inline-block;"
      "  width: 1em; height: 1em;"
      "  writing-mode: horizontal-tb;"
      "}"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root>a<c id=target>0</c>b</div>");

  EXPECT_EQ(R"DUMP(
{Line #descendants=4 LTR Standard} "0,0 110x300"
{Text 0-1 LTR Standard} "5,0 100x100"
{Box #descendants=1 AtomicInlineLTR Standard} "5,100 100x100"
{Text 2-3 LTR Standard} "5,200 100x100"
)DUMP",
            AsFragmentItemsString(
                *To<LayoutBlockFlow>(GetLayoutObjectByElementId("root"))));

  EXPECT_EQ(R"DUMP(
{Line #descendants=2 LTR Standard} "0,0 100x110"
{Text 0-1 LTR Standard} "0,5 100x100"
)DUMP",
            AsFragmentItemsString(
                *To<LayoutBlockFlow>(GetLayoutObjectByElementId("target"))));
}

#undef MAYBE_VerticalAlignBottomReplaced
}  // namespace
}  // namespace blink
