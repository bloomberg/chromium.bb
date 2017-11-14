// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_inline_items_builder.h"

#include "core/layout/LayoutInline.h"
#include "core/layout/ng/inline/ng_offset_mapping_builder.h"
#include "core/style/ComputedStyle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

static scoped_refptr<ComputedStyle> CreateWhitespaceStyle(
    EWhiteSpace whitespace) {
  scoped_refptr<ComputedStyle> style(ComputedStyle::Create());
  style->SetWhiteSpace(whitespace);
  return style;
}

static String GetCollapsed(const NGOffsetMappingBuilder& builder) {
  Vector<unsigned> mapping = builder.DumpOffsetMappingForTesting();

  Vector<unsigned> collapsed_indexes;
  for (unsigned i = 0; i + 1 < mapping.size(); ++i) {
    if (mapping[i] == mapping[i + 1])
      collapsed_indexes.push_back(i);
  }

  StringBuilder result;
  result.Append('{');
  bool first = true;
  for (unsigned index : collapsed_indexes) {
    if (!first)
      result.Append(", ");
    result.AppendNumber(index);
    first = false;
  }
  result.Append('}');
  return result.ToString();
}

class NGInlineItemsBuilderTest : public ::testing::Test {
 protected:
  void SetUp() override { style_ = ComputedStyle::Create(); }

  void SetWhiteSpace(EWhiteSpace whitespace) {
    style_->SetWhiteSpace(whitespace);
  }

  const String& TestAppend(const String inputs[], int size) {
    items_.clear();
    NGInlineItemsBuilderForOffsetMapping builder(&items_);
    for (int i = 0; i < size; i++)
      builder.Append(inputs[i], style_.get());
    text_ = builder.ToString();
    collapsed_ = GetCollapsed(builder.GetOffsetMappingBuilder());
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
  String collapsed_;
  scoped_refptr<ComputedStyle> style_;
};

#define TestWhitespaceValue(expected_text, expected_collapsed, input,         \
                            whitespace)                                       \
  SetWhiteSpace(whitespace);                                                  \
  EXPECT_EQ(expected_text, TestAppend(input)) << "white-space: " #whitespace; \
  EXPECT_EQ(expected_collapsed, collapsed_);

TEST_F(NGInlineItemsBuilderTest, CollapseSpaces) {
  String input("text text  text   text");
  String collapsed("text text text text");
  String collapsed_indexes("{10, 16, 17}");
  TestWhitespaceValue(collapsed, collapsed_indexes, input,
                      EWhiteSpace::kNormal);
  TestWhitespaceValue(collapsed, collapsed_indexes, input,
                      EWhiteSpace::kNowrap);
  TestWhitespaceValue(collapsed, collapsed_indexes, input,
                      EWhiteSpace::kWebkitNowrap);
  TestWhitespaceValue(collapsed, collapsed_indexes, input,
                      EWhiteSpace::kPreLine);
  TestWhitespaceValue(input, "{}", input, EWhiteSpace::kPre);
  TestWhitespaceValue(input, "{}", input, EWhiteSpace::kPreWrap);
}

TEST_F(NGInlineItemsBuilderTest, CollapseTabs) {
  String input("text text  text   text");
  String collapsed("text text text text");
  String collapsed_indexes("{10, 16, 17}");
  TestWhitespaceValue(collapsed, collapsed_indexes, input,
                      EWhiteSpace::kNormal);
  TestWhitespaceValue(collapsed, collapsed_indexes, input,
                      EWhiteSpace::kNowrap);
  TestWhitespaceValue(collapsed, collapsed_indexes, input,
                      EWhiteSpace::kWebkitNowrap);
  TestWhitespaceValue(collapsed, collapsed_indexes, input,
                      EWhiteSpace::kPreLine);
  TestWhitespaceValue(input, "{}", input, EWhiteSpace::kPre);
  TestWhitespaceValue(input, "{}", input, EWhiteSpace::kPreWrap);
}

TEST_F(NGInlineItemsBuilderTest, CollapseNewLines) {
  String input("text\ntext \n text\n\ntext");
  String collapsed("text text text text");
  String collapsed_indexes("{10, 11, 17}");
  TestWhitespaceValue(collapsed, collapsed_indexes, input,
                      EWhiteSpace::kNormal);
  TestWhitespaceValue(collapsed, collapsed_indexes, input,
                      EWhiteSpace::kNowrap);
  TestWhitespaceValue("text\ntext\ntext\n\ntext", "{9, 11}", input,
                      EWhiteSpace::kPreLine);
  TestWhitespaceValue(input, "{}", input, EWhiteSpace::kPre);
  TestWhitespaceValue(input, "{}", input, EWhiteSpace::kPreWrap);
}

TEST_F(NGInlineItemsBuilderTest, CollapseNewlinesAsSpaces) {
  EXPECT_EQ("text text", TestAppend("text\ntext"));
  EXPECT_EQ("{}", collapsed_);
  EXPECT_EQ("text text", TestAppend("text\n\ntext"));
  EXPECT_EQ("{5}", collapsed_);
  EXPECT_EQ("text text", TestAppend("text \n\n text"));
  EXPECT_EQ("{5, 6, 7}", collapsed_);
  EXPECT_EQ("text text", TestAppend("text \n \n text"));
  EXPECT_EQ("{5, 6, 7, 8}", collapsed_);
}

TEST_F(NGInlineItemsBuilderTest, CollapseAcrossElements) {
  EXPECT_EQ("text text", TestAppend("text ", " text"))
      << "Spaces are collapsed even when across elements.";
  EXPECT_EQ("{5}", collapsed_);
}

TEST_F(NGInlineItemsBuilderTest, CollapseLeadingSpaces) {
  EXPECT_EQ("text", TestAppend("  text"));
  EXPECT_EQ("{0, 1}", collapsed_);
  EXPECT_EQ("text", TestAppend(" ", "text"));
  EXPECT_EQ("{0}", collapsed_);
  EXPECT_EQ("text", TestAppend(" ", " text"));
  EXPECT_EQ("{0, 1}", collapsed_);
}

TEST_F(NGInlineItemsBuilderTest, CollapseTrailingSpaces) {
  EXPECT_EQ("text", TestAppend("text  "));
  EXPECT_EQ("{4, 5}", collapsed_);
  EXPECT_EQ("text", TestAppend("text", " "));
  EXPECT_EQ("{4}", collapsed_);
  EXPECT_EQ("text", TestAppend("text ", " "));
  EXPECT_EQ("{4, 5}", collapsed_);
}

TEST_F(NGInlineItemsBuilderTest, CollapseAllSpaces) {
  EXPECT_EQ("", TestAppend("  "));
  EXPECT_EQ("{0, 1}", collapsed_);
  EXPECT_EQ("", TestAppend("  ", "  "));
  EXPECT_EQ("{0, 1, 2, 3}", collapsed_);
  EXPECT_EQ("", TestAppend("  ", "\n"));
  EXPECT_EQ("{0, 1, 2}", collapsed_);
  EXPECT_EQ("", TestAppend("\n", "  "));
  EXPECT_EQ("{0, 1, 2}", collapsed_);
}

TEST_F(NGInlineItemsBuilderTest, CollapseLeadingNewlines) {
  EXPECT_EQ("text", TestAppend("\ntext"));
  EXPECT_EQ("{0}", collapsed_);
  EXPECT_EQ("text", TestAppend("\n\ntext"));
  EXPECT_EQ("{0, 1}", collapsed_);
  EXPECT_EQ("text", TestAppend("\n", "text"));
  EXPECT_EQ("{0}", collapsed_);
  EXPECT_EQ("text", TestAppend("\n\n", "text"));
  EXPECT_EQ("{0, 1}", collapsed_);
  EXPECT_EQ("text", TestAppend(" \n", "text"));
  EXPECT_EQ("{0, 1}", collapsed_);
  EXPECT_EQ("text", TestAppend("\n", " text"));
  EXPECT_EQ("{0, 1}", collapsed_);
  EXPECT_EQ("text", TestAppend("\n\n", " text"));
  EXPECT_EQ("{0, 1, 2}", collapsed_);
  EXPECT_EQ("text", TestAppend(" \n", " text"));
  EXPECT_EQ("{0, 1, 2}", collapsed_);
  EXPECT_EQ("text", TestAppend("\n", "\ntext"));
  EXPECT_EQ("{0, 1}", collapsed_);
  EXPECT_EQ("text", TestAppend("\n\n", "\ntext"));
  EXPECT_EQ("{0, 1, 2}", collapsed_);
  EXPECT_EQ("text", TestAppend(" \n", "\ntext"));
  EXPECT_EQ("{0, 1, 2}", collapsed_);
}

TEST_F(NGInlineItemsBuilderTest, CollapseTrailingNewlines) {
  EXPECT_EQ("text", TestAppend("text\n"));
  EXPECT_EQ("{4}", collapsed_);
  EXPECT_EQ("text", TestAppend("text", "\n"));
  EXPECT_EQ("{4}", collapsed_);
  EXPECT_EQ("text", TestAppend("text\n", "\n"));
  EXPECT_EQ("{4, 5}", collapsed_);
  EXPECT_EQ("text", TestAppend("text\n", " "));
  EXPECT_EQ("{4, 5}", collapsed_);
  EXPECT_EQ("text", TestAppend("text ", "\n"));
  EXPECT_EQ("{4, 5}", collapsed_);
}

TEST_F(NGInlineItemsBuilderTest, CollapseBeforeNewlineAcrossElements) {
  EXPECT_EQ("text text", TestAppend("text ", "\ntext"));
  EXPECT_EQ("{5}", collapsed_);
  EXPECT_EQ("text text", TestAppend("text", " ", "\ntext"));
  EXPECT_EQ("{5}", collapsed_);
}

TEST_F(NGInlineItemsBuilderTest, CollapseBeforeAndAfterNewline) {
  SetWhiteSpace(EWhiteSpace::kPreLine);
  EXPECT_EQ("text\ntext", TestAppend("text  \n  text"))
      << "Spaces before and after newline are removed.";
  EXPECT_EQ("{4, 5, 7, 8}", collapsed_);
}

TEST_F(NGInlineItemsBuilderTest,
       CollapsibleSpaceAfterNonCollapsibleSpaceAcrossElements) {
  NGInlineItemsBuilderForOffsetMapping builder(&items_);
  scoped_refptr<ComputedStyle> pre_wrap(
      CreateWhitespaceStyle(EWhiteSpace::kPreWrap));
  builder.Append("text ", pre_wrap.get());
  builder.Append(" text", style_.get());
  EXPECT_EQ("text  text", builder.ToString())
      << "The whitespace in constructions like '<span style=\"white-space: "
         "pre-wrap\">text <span><span> text</span>' does not collapse.";
  EXPECT_EQ("{}", GetCollapsed(builder.GetOffsetMappingBuilder()));
}

TEST_F(NGInlineItemsBuilderTest, CollapseZeroWidthSpaces) {
  EXPECT_EQ(String(u"text\u200Btext"), TestAppend(u"text\u200B\ntext"))
      << "Newline is removed if the character before is ZWS.";
  EXPECT_EQ("{5}", collapsed_);
  EXPECT_EQ(String(u"text\u200Btext"), TestAppend(u"text\n\u200Btext"))
      << "Newline is removed if the character after is ZWS.";
  EXPECT_EQ("{4}", collapsed_);
  EXPECT_EQ(String(u"text\u200B\u200Btext"),
            TestAppend(u"text\u200B\n\u200Btext"))
      << "Newline is removed if the character before/after is ZWS.";
  EXPECT_EQ("{5}", collapsed_);

  EXPECT_EQ(String(u"text\u200Btext"), TestAppend(u"text\n", u"\u200Btext"))
      << "Newline is removed if the character after across elements is ZWS.";
  EXPECT_EQ("{4}", collapsed_);
  EXPECT_EQ(String(u"text\u200Btext"), TestAppend(u"text\u200B", u"\ntext"))
      << "Newline is removed if the character before is ZWS even across "
         "elements.";
  EXPECT_EQ("{5}", collapsed_);

  EXPECT_EQ(String(u"text\u200Btext"), TestAppend(u"text \n", u"\u200Btext"))
      << "Collapsible space before newline does not affect the result.";
  EXPECT_EQ("{4, 5}", collapsed_);

  EXPECT_EQ(String(u"text\u200Btext"), TestAppend(u"text\u200B\n", u" text"))
      << "Collapsible space after newline is removed even when the "
         "newline was removed.";
  EXPECT_EQ("{5, 6}", collapsed_);
}

TEST_F(NGInlineItemsBuilderTest, CollapseEastAsianWidth) {
  EXPECT_EQ(String(u"\u4E00\u4E00"), TestAppend(u"\u4E00\n\u4E00"))
      << "Newline is removed when both sides are Wide.";
  EXPECT_EQ("{1}", collapsed_);

  EXPECT_EQ(String(u"\u4E00 A"), TestAppend(u"\u4E00\nA"))
      << "Newline is not removed when after is Narrow.";
  EXPECT_EQ("{}", collapsed_);
  EXPECT_EQ(String(u"A \u4E00"), TestAppend(u"A\n\u4E00"))
      << "Newline is not removed when before is Narrow.";
  EXPECT_EQ("{}", collapsed_);

  EXPECT_EQ(String(u"\u4E00\u4E00"), TestAppend(u"\u4E00\n", u"\u4E00"))
      << "Newline at the end of elements is removed when both sides are Wide.";
  EXPECT_EQ("{1}", collapsed_);
  EXPECT_EQ(String(u"\u4E00\u4E00"), TestAppend(u"\u4E00", u"\n\u4E00"))
      << "Newline at the beginning of elements is removed "
         "when both sides are Wide.";
  EXPECT_EQ("{1}", collapsed_);
}

TEST_F(NGInlineItemsBuilderTest, OpaqueToSpaceCollapsing) {
  NGInlineItemsBuilderForOffsetMapping builder(&items_);
  builder.Append("Hello ", style_.get());
  builder.AppendOpaque(NGInlineItem::kBidiControl,
                       kFirstStrongIsolateCharacter);
  builder.Append(" ", style_.get());
  builder.AppendOpaque(NGInlineItem::kBidiControl,
                       kFirstStrongIsolateCharacter);
  builder.Append(" World", style_.get());
  EXPECT_EQ(String(u"Hello \u2068\u2068World"), builder.ToString());
  EXPECT_EQ("{7, 9}", GetCollapsed(builder.GetOffsetMappingBuilder()));
}

TEST_F(NGInlineItemsBuilderTest, CollapseAroundReplacedElement) {
  NGInlineItemsBuilderForOffsetMapping builder(&items_);
  builder.Append("Hello ", style_.get());
  builder.AppendAtomicInline();
  builder.Append(" World", style_.get());
  EXPECT_EQ(String(u"Hello \uFFFC World"), builder.ToString());
  EXPECT_EQ("{}", GetCollapsed(builder.GetOffsetMappingBuilder()));
}

TEST_F(NGInlineItemsBuilderTest, CollapseNewlineAfterObject) {
  NGInlineItemsBuilderForOffsetMapping builder(&items_);
  builder.AppendAtomicInline();
  builder.Append("\n", style_.get());
  builder.AppendAtomicInline();
  EXPECT_EQ(String(u"\uFFFC \uFFFC"), builder.ToString());
  EXPECT_EQ(3u, items_.size());
  EXPECT_EQ(nullptr, items_[0].Style());
  EXPECT_EQ(style_.get(), items_[1].Style());
  EXPECT_EQ(nullptr, items_[2].Style());
  EXPECT_EQ("{}", GetCollapsed(builder.GetOffsetMappingBuilder()));
}

TEST_F(NGInlineItemsBuilderTest, AppendEmptyString) {
  EXPECT_EQ("", TestAppend(""));
  EXPECT_EQ("{}", collapsed_);
  EXPECT_EQ(0u, items_.size());
}

TEST_F(NGInlineItemsBuilderTest, NewLines) {
  SetWhiteSpace(EWhiteSpace::kPre);
  EXPECT_EQ("apple\norange\ngrape\n", TestAppend("apple\norange\ngrape\n"));
  EXPECT_EQ("{}", collapsed_);
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
  NGInlineItemsBuilderForOffsetMapping builder(&items);
  scoped_refptr<ComputedStyle> block_style(ComputedStyle::Create());
  builder.EnterBlock(block_style.get());
  builder.ExitBlock();

  EXPECT_EQ("", builder.ToString());
  EXPECT_EQ("{}", GetCollapsed(builder.GetOffsetMappingBuilder()));
}

TEST_F(NGInlineItemsBuilderTest, BidiBlockOverride) {
  Vector<NGInlineItem> items;
  NGInlineItemsBuilderForOffsetMapping builder(&items);
  scoped_refptr<ComputedStyle> block_style(ComputedStyle::Create());
  block_style->SetUnicodeBidi(UnicodeBidi::kBidiOverride);
  block_style->SetDirection(TextDirection::kRtl);
  builder.EnterBlock(block_style.get());
  builder.Append("Hello", style_.get());
  builder.ExitBlock();

  // Expected control characters as defined in:
  // https://drafts.csswg.org/css-writing-modes-3/#bidi-control-codes-injection-table
  EXPECT_EQ(String(u"\u202E"
                   u"Hello"
                   u"\u202C"),
            builder.ToString());
  EXPECT_EQ("{}", GetCollapsed(builder.GetOffsetMappingBuilder()));
}

static std::unique_ptr<LayoutInline> CreateLayoutInline(
    void (*initialize_style)(ComputedStyle*)) {
  scoped_refptr<ComputedStyle> style(ComputedStyle::Create());
  initialize_style(style.get());
  std::unique_ptr<LayoutInline> node = std::make_unique<LayoutInline>(nullptr);
  node->SetStyleInternal(std::move(style));
  return node;
}

TEST_F(NGInlineItemsBuilderTest, BidiIsolate) {
  Vector<NGInlineItem> items;
  NGInlineItemsBuilderForOffsetMapping builder(&items);
  builder.Append("Hello ", style_.get());
  std::unique_ptr<LayoutInline> isolate_rtl(
      CreateLayoutInline([](ComputedStyle* style) {
        style->SetUnicodeBidi(UnicodeBidi::kIsolate);
        style->SetDirection(TextDirection::kRtl);
      }));
  builder.EnterInline(isolate_rtl.get());
  builder.Append(u"\u05E2\u05D1\u05E8\u05D9\u05EA", style_.get());
  builder.ExitInline(isolate_rtl.get());
  builder.Append(" World", style_.get());

  // Expected control characters as defined in:
  // https://drafts.csswg.org/css-writing-modes-3/#bidi-control-codes-injection-table
  EXPECT_EQ(String(u"Hello "
                   u"\u2067"
                   u"\u05E2\u05D1\u05E8\u05D9\u05EA"
                   u"\u2069"
                   u" World"),
            builder.ToString());
  EXPECT_EQ("{}", GetCollapsed(builder.GetOffsetMappingBuilder()));
}

TEST_F(NGInlineItemsBuilderTest, BidiIsolateOverride) {
  Vector<NGInlineItem> items;
  NGInlineItemsBuilderForOffsetMapping builder(&items);
  builder.Append("Hello ", style_.get());
  std::unique_ptr<LayoutInline> isolate_override_rtl(
      CreateLayoutInline([](ComputedStyle* style) {
        style->SetUnicodeBidi(UnicodeBidi::kIsolateOverride);
        style->SetDirection(TextDirection::kRtl);
      }));
  builder.EnterInline(isolate_override_rtl.get());
  builder.Append(u"\u05E2\u05D1\u05E8\u05D9\u05EA", style_.get());
  builder.ExitInline(isolate_override_rtl.get());
  builder.Append(" World", style_.get());

  // Expected control characters as defined in:
  // https://drafts.csswg.org/css-writing-modes-3/#bidi-control-codes-injection-table
  EXPECT_EQ(String(u"Hello "
                   u"\u2068\u202E"
                   u"\u05E2\u05D1\u05E8\u05D9\u05EA"
                   u"\u202C\u2069"
                   u" World"),
            builder.ToString());
  EXPECT_EQ("{}", GetCollapsed(builder.GetOffsetMappingBuilder()));
}

}  // namespace

}  // namespace blink
