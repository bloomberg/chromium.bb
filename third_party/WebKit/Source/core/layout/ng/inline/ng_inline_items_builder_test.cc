// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_inline_items_builder.h"

#include "core/layout/LayoutInline.h"
#include "core/layout/ng/inline/ng_inline_node.h"
#include "core/style/ComputedStyle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

static PassRefPtr<ComputedStyle> CreateWhitespaceStyle(EWhiteSpace whitespace) {
  RefPtr<ComputedStyle> style(ComputedStyle::Create());
  style->SetWhiteSpace(whitespace);
  return style.Release();
}

class NGInlineItemsBuilderTest : public ::testing::Test {
 protected:
  void SetUp() override { style_ = ComputedStyle::Create(); }

  void SetWhiteSpace(EWhiteSpace whitespace) {
    style_->SetWhiteSpace(whitespace);
  }

  const String& TestAppend(const String inputs[], int size) {
    items_.clear();
    NGInlineItemsBuilder builder(&items_);
    for (int i = 0; i < size; i++)
      builder.Append(inputs[i], style_.Get());
    text_ = builder.ToString();
    ValidateItems();
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

  const String& TestAppend(const String& input1,
                           const String& input2,
                           const String& input3) {
    String inputs[] = {input1, input2, input3};
    return TestAppend(inputs, 3);
  }

  void ValidateItems() {
    unsigned current_offset = 0;
    for (unsigned i = 0; i < items_.size(); i++) {
      const NGInlineItem& item = items_[i];
      EXPECT_EQ(current_offset, item.StartOffset());
      EXPECT_LT(item.StartOffset(), item.EndOffset());
      current_offset = item.EndOffset();
    }
    EXPECT_EQ(current_offset, text_.length());
  }

  Vector<NGInlineItem> items_;
  String text_;
  RefPtr<ComputedStyle> style_;
};

#define TestWhitespaceValue(expected, input, whitespace) \
  SetWhiteSpace(whitespace);                             \
  EXPECT_EQ(expected, TestAppend(input)) << "white-space: " #whitespace;

TEST_F(NGInlineItemsBuilderTest, CollapseSpaces) {
  String input("text text  text   text");
  String collapsed("text text text text");
  TestWhitespaceValue(collapsed, input, EWhiteSpace::kNormal);
  TestWhitespaceValue(collapsed, input, EWhiteSpace::kNowrap);
  TestWhitespaceValue(collapsed, input, EWhiteSpace::kWebkitNowrap);
  TestWhitespaceValue(collapsed, input, EWhiteSpace::kPreLine);
  TestWhitespaceValue(input, input, EWhiteSpace::kPre);
  TestWhitespaceValue(input, input, EWhiteSpace::kPreWrap);
}

TEST_F(NGInlineItemsBuilderTest, CollapseTabs) {
  String input("text\ttext\t text \t text");
  String collapsed("text text text text");
  TestWhitespaceValue(collapsed, input, EWhiteSpace::kNormal);
  TestWhitespaceValue(collapsed, input, EWhiteSpace::kNowrap);
  TestWhitespaceValue(collapsed, input, EWhiteSpace::kWebkitNowrap);
  TestWhitespaceValue(collapsed, input, EWhiteSpace::kPreLine);
  TestWhitespaceValue(input, input, EWhiteSpace::kPre);
  TestWhitespaceValue(input, input, EWhiteSpace::kPreWrap);
}

TEST_F(NGInlineItemsBuilderTest, CollapseNewLines) {
  String input("text\ntext \n text\n\ntext");
  String collapsed("text text text text");
  TestWhitespaceValue(collapsed, input, EWhiteSpace::kNormal);
  TestWhitespaceValue(collapsed, input, EWhiteSpace::kNowrap);
  TestWhitespaceValue("text\ntext\ntext\n\ntext", input, EWhiteSpace::kPreLine);
  TestWhitespaceValue(input, input, EWhiteSpace::kPre);
  TestWhitespaceValue(input, input, EWhiteSpace::kPreWrap);
}

TEST_F(NGInlineItemsBuilderTest, CollapseNewlinesAsSpaces) {
  EXPECT_EQ("text text", TestAppend("text\ntext"));
  EXPECT_EQ("text text", TestAppend("text\n\ntext"));
  EXPECT_EQ("text text", TestAppend("text \n\n text"));
  EXPECT_EQ("text text", TestAppend("text \n \n text"));
}

TEST_F(NGInlineItemsBuilderTest, CollapseAcrossElements) {
  EXPECT_EQ("text text", TestAppend("text ", " text"))
      << "Spaces are collapsed even when across elements.";
}

TEST_F(NGInlineItemsBuilderTest, CollapseLeadingSpaces) {
  EXPECT_EQ("text", TestAppend("  text"));
  EXPECT_EQ("text", TestAppend(" ", "text"));
  EXPECT_EQ("text", TestAppend(" ", " text"));
}

TEST_F(NGInlineItemsBuilderTest, CollapseTrailingSpaces) {
  EXPECT_EQ("text", TestAppend("text  "));
  EXPECT_EQ("text", TestAppend("text", " "));
  EXPECT_EQ("text", TestAppend("text ", " "));
}

TEST_F(NGInlineItemsBuilderTest, CollapseAllSpaces) {
  EXPECT_EQ("", TestAppend("  "));
  EXPECT_EQ("", TestAppend("  ", "  "));
  EXPECT_EQ("", TestAppend("  ", "\n"));
  EXPECT_EQ("", TestAppend("\n", "  "));
}

TEST_F(NGInlineItemsBuilderTest, CollapseLeadingNewlines) {
  EXPECT_EQ("text", TestAppend("\ntext"));
  EXPECT_EQ("text", TestAppend("\n\ntext"));
  EXPECT_EQ("text", TestAppend("\n", "text"));
  EXPECT_EQ("text", TestAppend("\n\n", "text"));
  EXPECT_EQ("text", TestAppend(" \n", "text"));
  EXPECT_EQ("text", TestAppend("\n", " text"));
  EXPECT_EQ("text", TestAppend("\n\n", " text"));
  EXPECT_EQ("text", TestAppend(" \n", " text"));
  EXPECT_EQ("text", TestAppend("\n", "\ntext"));
  EXPECT_EQ("text", TestAppend("\n\n", "\ntext"));
  EXPECT_EQ("text", TestAppend(" \n", "\ntext"));
}

TEST_F(NGInlineItemsBuilderTest, CollapseTrailingNewlines) {
  EXPECT_EQ("text", TestAppend("text\n"));
  EXPECT_EQ("text", TestAppend("text", "\n"));
  EXPECT_EQ("text", TestAppend("text\n", "\n"));
  EXPECT_EQ("text", TestAppend("text\n", " "));
  EXPECT_EQ("text", TestAppend("text ", "\n"));
}

TEST_F(NGInlineItemsBuilderTest, CollapseBeforeNewlineAcrossElements) {
  EXPECT_EQ("text text", TestAppend("text ", "\ntext"));
  EXPECT_EQ("text text", TestAppend("text", " ", "\ntext"));
}

TEST_F(NGInlineItemsBuilderTest, CollapseBeforeAndAfterNewline) {
  SetWhiteSpace(EWhiteSpace::kPreLine);
  EXPECT_EQ("text\ntext", TestAppend("text  \n  text"))
      << "Spaces before and after newline are removed.";
}

TEST_F(NGInlineItemsBuilderTest,
       CollapsibleSpaceAfterNonCollapsibleSpaceAcrossElements) {
  NGInlineItemsBuilder builder(&items_);
  RefPtr<ComputedStyle> pre_wrap(CreateWhitespaceStyle(EWhiteSpace::kPreWrap));
  builder.Append("text ", pre_wrap.Get());
  builder.Append(" text", style_.Get());
  EXPECT_EQ("text  text", builder.ToString())
      << "The whitespace in constructions like '<span style=\"white-space: "
         "pre-wrap\">text <span><span> text</span>' does not collapse.";
}

TEST_F(NGInlineItemsBuilderTest, CollapseZeroWidthSpaces) {
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

TEST_F(NGInlineItemsBuilderTest, CollapseEastAsianWidth) {
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

TEST_F(NGInlineItemsBuilderTest, CollapseAroundReplacedElement) {
  NGInlineItemsBuilder builder(&items_);
  builder.Append("Hello ", style_.Get());
  builder.Append(NGInlineItem::kAtomicInline, kObjectReplacementCharacter);
  builder.Append(" World", style_.Get());
  EXPECT_EQ(String(u"Hello \uFFFC World"), builder.ToString());
}

TEST_F(NGInlineItemsBuilderTest, CollapseNewlineAfterObject) {
  NGInlineItemsBuilder builder(&items_);
  builder.Append(NGInlineItem::kAtomicInline, kObjectReplacementCharacter);
  builder.Append("\n", style_.Get());
  builder.Append(NGInlineItem::kAtomicInline, kObjectReplacementCharacter);
  EXPECT_EQ(String(u"\uFFFC \uFFFC"), builder.ToString());
  EXPECT_EQ(3u, items_.size());
  EXPECT_EQ(nullptr, items_[0].Style());
  EXPECT_EQ(style_.Get(), items_[1].Style());
  EXPECT_EQ(nullptr, items_[2].Style());
}

TEST_F(NGInlineItemsBuilderTest, AppendEmptyString) {
  EXPECT_EQ("", TestAppend(""));
  EXPECT_EQ(0u, items_.size());
}

TEST_F(NGInlineItemsBuilderTest, NewLines) {
  SetWhiteSpace(EWhiteSpace::kPre);
  EXPECT_EQ("apple\norange\ngrape\n", TestAppend("apple\norange\ngrape\n"));
  EXPECT_EQ(6u, items_.size());
  EXPECT_EQ(NGInlineItem::kText, items_[0].Type());
  EXPECT_EQ(NGInlineItem::kControl, items_[1].Type());
  EXPECT_EQ(NGInlineItem::kText, items_[2].Type());
  EXPECT_EQ(NGInlineItem::kControl, items_[3].Type());
  EXPECT_EQ(NGInlineItem::kText, items_[4].Type());
  EXPECT_EQ(NGInlineItem::kControl, items_[5].Type());
}

TEST_F(NGInlineItemsBuilderTest, Empty) {
  Vector<NGInlineItem> items;
  NGInlineItemsBuilder builder(&items);
  RefPtr<ComputedStyle> block_style(ComputedStyle::Create());
  builder.EnterBlock(block_style.Get());
  builder.ExitBlock();

  EXPECT_EQ("", builder.ToString());
}

TEST_F(NGInlineItemsBuilderTest, BidiBlockOverride) {
  Vector<NGInlineItem> items;
  NGInlineItemsBuilder builder(&items);
  RefPtr<ComputedStyle> block_style(ComputedStyle::Create());
  block_style->SetUnicodeBidi(UnicodeBidi::kBidiOverride);
  block_style->SetDirection(TextDirection::kRtl);
  builder.EnterBlock(block_style.Get());
  builder.Append("Hello", style_.Get());
  builder.ExitBlock();

  // Expected control characters as defined in:
  // https://drafts.csswg.org/css-writing-modes-3/#bidi-control-codes-injection-table
  EXPECT_EQ(String(u"\u202E"
                   u"Hello"
                   u"\u202C"),
            builder.ToString());
}

static std::unique_ptr<LayoutInline> CreateLayoutInline(
    void (*initialize_style)(ComputedStyle*)) {
  RefPtr<ComputedStyle> style(ComputedStyle::Create());
  initialize_style(style.Get());
  std::unique_ptr<LayoutInline> node = WTF::MakeUnique<LayoutInline>(nullptr);
  node->SetStyleInternal(std::move(style));
  return node;
}

TEST_F(NGInlineItemsBuilderTest, BidiIsolate) {
  Vector<NGInlineItem> items;
  NGInlineItemsBuilder builder(&items);
  builder.Append("Hello ", style_.Get());
  std::unique_ptr<LayoutInline> isolate_rtl(
      CreateLayoutInline([](ComputedStyle* style) {
        style->SetUnicodeBidi(UnicodeBidi::kIsolate);
        style->SetDirection(TextDirection::kRtl);
      }));
  builder.EnterInline(isolate_rtl.get());
  builder.Append(u"\u05E2\u05D1\u05E8\u05D9\u05EA", style_.Get());
  builder.ExitInline(isolate_rtl.get());
  builder.Append(" World", style_.Get());

  // Expected control characters as defined in:
  // https://drafts.csswg.org/css-writing-modes-3/#bidi-control-codes-injection-table
  EXPECT_EQ(String(u"Hello "
                   u"\u2067"
                   u"\u05E2\u05D1\u05E8\u05D9\u05EA"
                   u"\u2069"
                   u" World"),
            builder.ToString());
}

TEST_F(NGInlineItemsBuilderTest, BidiIsolateOverride) {
  Vector<NGInlineItem> items;
  NGInlineItemsBuilder builder(&items);
  builder.Append("Hello ", style_.Get());
  std::unique_ptr<LayoutInline> isolate_override_rtl(
      CreateLayoutInline([](ComputedStyle* style) {
        style->SetUnicodeBidi(UnicodeBidi::kIsolateOverride);
        style->SetDirection(TextDirection::kRtl);
      }));
  builder.EnterInline(isolate_override_rtl.get());
  builder.Append(u"\u05E2\u05D1\u05E8\u05D9\u05EA", style_.Get());
  builder.ExitInline(isolate_override_rtl.get());
  builder.Append(" World", style_.Get());

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
