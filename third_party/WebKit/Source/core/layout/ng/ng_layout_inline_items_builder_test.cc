// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_layout_inline_items_builder.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/ng/ng_inline_node.h"
#include "core/style/ComputedStyle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

static PassRefPtr<ComputedStyle> CreateWhitespaceStyle(EWhiteSpace whitespace) {
  RefPtr<ComputedStyle> style(ComputedStyle::create());
  style->setWhiteSpace(whitespace);
  return style.release();
}

class NGLayoutInlineItemsBuilderTest : public ::testing::Test {
 protected:
  void SetUp() override { style_ = ComputedStyle::create(); }

  void SetWhiteSpace(EWhiteSpace whitespace) {
    style_->setWhiteSpace(whitespace);
  }

  const String& TestAppend(const String inputs[], int size) {
    items_.clear();
    NGLayoutInlineItemsBuilder builder(&items_);
    for (int i = 0; i < size; i++)
      builder.Append(inputs[i], style_.get());
    text_ = builder.ToString();
    return text_;
  }

  const String& TestAppend(const String& input) {
    String inputs[] = {input};
    return TestAppend(inputs, 1);
  }

  const String& TestAppend(const String& input1, const String& input2) {
    String inputs[] = {input1, input2};
    return TestAppend(inputs, 2);
  }

  Vector<NGLayoutInlineItem> items_;
  String text_;
  RefPtr<ComputedStyle> style_;
};

#define TestWhitespaceValue(expected, input, whitespace) \
  SetWhiteSpace(whitespace);                             \
  EXPECT_EQ(expected, TestAppend(input)) << "white-space: " #whitespace;

TEST_F(NGLayoutInlineItemsBuilderTest, CollapseSpaces) {
  String input("text text  text   text");
  String collapsed("text text text text");
  TestWhitespaceValue(collapsed, input, EWhiteSpace::Normal);
  TestWhitespaceValue(collapsed, input, EWhiteSpace::Nowrap);
  TestWhitespaceValue(collapsed, input, EWhiteSpace::KhtmlNowrap);
  TestWhitespaceValue(collapsed, input, EWhiteSpace::PreLine);
  TestWhitespaceValue(input, input, EWhiteSpace::Pre);
  TestWhitespaceValue(input, input, EWhiteSpace::PreWrap);
}

TEST_F(NGLayoutInlineItemsBuilderTest, CollapseTabs) {
  String input("text\ttext\t text \t text");
  String collapsed("text text text text");
  TestWhitespaceValue(collapsed, input, EWhiteSpace::Normal);
  TestWhitespaceValue(collapsed, input, EWhiteSpace::Nowrap);
  TestWhitespaceValue(collapsed, input, EWhiteSpace::KhtmlNowrap);
  TestWhitespaceValue(collapsed, input, EWhiteSpace::PreLine);
  TestWhitespaceValue(input, input, EWhiteSpace::Pre);
  TestWhitespaceValue(input, input, EWhiteSpace::PreWrap);
}

TEST_F(NGLayoutInlineItemsBuilderTest, CollapseNewLines) {
  String input("text\ntext \n text");
  String collapsed("text text text");
  TestWhitespaceValue(collapsed, input, EWhiteSpace::Normal);
  TestWhitespaceValue(collapsed, input, EWhiteSpace::Nowrap);
  TestWhitespaceValue("text\ntext\ntext", input, EWhiteSpace::PreLine);
  TestWhitespaceValue(input, input, EWhiteSpace::Pre);
  TestWhitespaceValue(input, input, EWhiteSpace::PreWrap);
}

TEST_F(NGLayoutInlineItemsBuilderTest, CollapseAcrossElements) {
  EXPECT_EQ("text text", TestAppend("text ", " text"))
      << "Spaces are collapsed even when across elements.";
}

TEST_F(NGLayoutInlineItemsBuilderTest, CollapseLeadingSpaces) {
  EXPECT_EQ("text", TestAppend("  text")) << "Leading spaces are removed.";
}

TEST_F(NGLayoutInlineItemsBuilderTest, CollapseBeforeAndAfterNewline) {
  SetWhiteSpace(EWhiteSpace::PreLine);
  EXPECT_EQ("text\ntext", TestAppend("text  \n  text"))
      << "Spaces before and after newline are removed.";
}

TEST_F(NGLayoutInlineItemsBuilderTest,
       CollapsibleSpaceAfterNonCollapsibleSpaceAcrossElements) {
  NGLayoutInlineItemsBuilder builder(&items_);
  RefPtr<ComputedStyle> pre_wrap(CreateWhitespaceStyle(EWhiteSpace::PreWrap));
  builder.Append("text ", pre_wrap.get());
  builder.Append(" text", style_.get());
  EXPECT_EQ("text  text", builder.ToString())
      << "The whitespace in constructions like '<span style=\"white-space: "
         "pre-wrap\">text <span><span> text</span>' does not collapse.";
}

TEST_F(NGLayoutInlineItemsBuilderTest, CollapseZeroWidthSpaces) {
  EXPECT_EQ("text text", TestAppend("text\ntext"))
      << "Newline is converted to a space.";
  EXPECT_EQ("text ", TestAppend("text\n"))
      << "Newline at end is converted to a space.";

  EXPECT_EQ(String(u"text\u200Btext"), TestAppend(u"text\u200B\ntext"))
      << "Newline is removed if the character before is ZWS.";
  EXPECT_EQ(String(u"text\u200Btext"), TestAppend(u"text\n\u200Btext"))
      << "Newline is removed if the character after is ZWS.";
  EXPECT_EQ(String(u"text\u200B\u200Btext"),
            TestAppend(u"text\u200B\n\u200Btext"))
      << "Newline is removed if the character before/after is ZWS.";

  EXPECT_EQ(String(u"text\u200Btext"), TestAppend(u"text\n", u"\u200Btext"))
      << "Newline is removed if the character after across elements is ZWS.";
  EXPECT_EQ(String(u"text\u200Btext"), TestAppend(u"text\u200B", u"\ntext"))
      << "Newline is removed if the character before is ZWS even across "
         "elements.";

  EXPECT_EQ(String(u"text\u200Btext"), TestAppend(u"text \n", u"\u200Btext"))
      << "Collapsible space before newline does not affect the result.";

  EXPECT_EQ(String(u"text\u200Btext"), TestAppend(u"text\u200B\n", u" text"))
      << "Collapsible space after newline is removed even when the "
         "newline was removed.";
}

TEST_F(NGLayoutInlineItemsBuilderTest, CollapseEastAsianWidth) {
  EXPECT_EQ(String(u"\u4E00\u4E00"), TestAppend(u"\u4E00\n\u4E00"))
      << "Newline is removed when both sides are Wide.";

  EXPECT_EQ(String(u"\u4E00 A"), TestAppend(u"\u4E00\nA"))
      << "Newline is not removed when after is Narrow.";
  EXPECT_EQ(String(u"A \u4E00"), TestAppend(u"A\n\u4E00"))
      << "Newline is not removed when before is Narrow.";

  EXPECT_EQ(String(u"\u4E00\u4E00"), TestAppend(u"\u4E00\n", u"\u4E00"))
      << "Newline at the end of elements is removed when both sides are Wide.";
  EXPECT_EQ(String(u"\u4E00\u4E00"), TestAppend(u"\u4E00", u"\n\u4E00"))
      << "Newline at the beginning of elements is removed "
         "when both sides are Wide.";
}

TEST_F(NGLayoutInlineItemsBuilderTest, CollapseAroundReplacedElement) {
  NGLayoutInlineItemsBuilder builder(&items_);
  builder.Append("Hello ", style_.get());
  builder.Append(objectReplacementCharacter);
  builder.Append(" World", style_.get());
  EXPECT_EQ(String(u"Hello \uFFFC World"), builder.ToString());
}

TEST_F(NGLayoutInlineItemsBuilderTest, AppendAsOpaqueToSpaceCollapsing) {
  NGLayoutInlineItemsBuilder builder(&items_);
  builder.Append("Hello ", style_.get());
  builder.AppendAsOpaqueToSpaceCollapsing(firstStrongIsolateCharacter);
  builder.Append(" World", style_.get());
  EXPECT_EQ(String(u"Hello \u2068World"), builder.ToString());
}

TEST_F(NGLayoutInlineItemsBuilderTest, Empty) {
  Vector<NGLayoutInlineItem> items;
  NGLayoutInlineItemsBuilder builder(&items);
  RefPtr<ComputedStyle> block_style(ComputedStyle::create());
  builder.EnterBlock(block_style.get());
  builder.ExitBlock();

  EXPECT_EQ("", builder.ToString());
}

TEST_F(NGLayoutInlineItemsBuilderTest, BidiBlockOverride) {
  Vector<NGLayoutInlineItem> items;
  NGLayoutInlineItemsBuilder builder(&items);
  RefPtr<ComputedStyle> block_style(ComputedStyle::create());
  block_style->setUnicodeBidi(Override);
  block_style->setDirection(RTL);
  builder.EnterBlock(block_style.get());
  builder.Append("Hello", style_.get());
  builder.ExitBlock();

  // Expected control characters as defined in:
  // https://drafts.csswg.org/css-writing-modes-3/#bidi-control-codes-injection-table
  EXPECT_EQ(String(u"\u202E"
                   u"Hello"
                   u"\u202C"),
            builder.ToString());
}

static std::unique_ptr<LayoutInline> createLayoutInline(
    void (*initialize_style)(ComputedStyle*)) {
  RefPtr<ComputedStyle> style(ComputedStyle::create());
  initialize_style(style.get());
  std::unique_ptr<LayoutInline> node = makeUnique<LayoutInline>(nullptr);
  node->setStyleInternal(std::move(style));
  return node;
}

TEST_F(NGLayoutInlineItemsBuilderTest, BidiIsolate) {
  Vector<NGLayoutInlineItem> items;
  NGLayoutInlineItemsBuilder builder(&items);
  builder.Append("Hello ", style_.get());
  std::unique_ptr<LayoutInline> isolateRTL(
      createLayoutInline([](ComputedStyle* style) {
        style->setUnicodeBidi(Isolate);
        style->setDirection(RTL);
      }));
  builder.EnterInline(isolateRTL.get());
  builder.Append(u"\u05E2\u05D1\u05E8\u05D9\u05EA", style_.get());
  builder.ExitInline(isolateRTL.get());
  builder.Append(" World", style_.get());

  // Expected control characters as defined in:
  // https://drafts.csswg.org/css-writing-modes-3/#bidi-control-codes-injection-table
  EXPECT_EQ(String(u"Hello "
                   u"\u2067"
                   u"\u05E2\u05D1\u05E8\u05D9\u05EA"
                   u"\u2069"
                   u" World"),
            builder.ToString());
}

TEST_F(NGLayoutInlineItemsBuilderTest, BidiIsolateOverride) {
  Vector<NGLayoutInlineItem> items;
  NGLayoutInlineItemsBuilder builder(&items);
  builder.Append("Hello ", style_.get());
  std::unique_ptr<LayoutInline> isolateOverrideRTL(
      createLayoutInline([](ComputedStyle* style) {
        style->setUnicodeBidi(IsolateOverride);
        style->setDirection(RTL);
      }));
  builder.EnterInline(isolateOverrideRTL.get());
  builder.Append(u"\u05E2\u05D1\u05E8\u05D9\u05EA", style_.get());
  builder.ExitInline(isolateOverrideRTL.get());
  builder.Append(" World", style_.get());

  // Expected control characters as defined in:
  // https://drafts.csswg.org/css-writing-modes-3/#bidi-control-codes-injection-table
  EXPECT_EQ(String(u"Hello "
                   u"\u2068\u202E"
                   u"\u05E2\u05D1\u05E8\u05D9\u05EA"
                   u"\u202C\u2069"
                   u" World"),
            builder.ToString());
}

}  // namespace

}  // namespace blink
