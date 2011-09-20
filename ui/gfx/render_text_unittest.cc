// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/render_text.h"

#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gfx {

class RenderTextTest : public testing::Test {
};

TEST_F(RenderTextTest, DefaultStyle) {
  // Defaults to empty text with no styles.
  scoped_ptr<RenderText> render_text(RenderText::CreateRenderText());
  EXPECT_TRUE(render_text->text().empty());
  EXPECT_TRUE(render_text->style_ranges().empty());

  // Test that the built-in default style is applied for new text.
  render_text->SetText(ASCIIToUTF16("abc"));
  EXPECT_EQ(1U, render_text->style_ranges().size());
  StyleRange style;
  EXPECT_EQ(style.font.GetFontName(),
            render_text->style_ranges()[0].font.GetFontName());
  EXPECT_EQ(style.foreground, render_text->style_ranges()[0].foreground);
  EXPECT_EQ(ui::Range(0, 3), render_text->style_ranges()[0].range);
  EXPECT_EQ(style.strike, render_text->style_ranges()[0].strike);
  EXPECT_EQ(style.underline, render_text->style_ranges()[0].underline);

  // Test that clearing the text also clears the styles.
  render_text->SetText(string16());
  EXPECT_TRUE(render_text->text().empty());
  EXPECT_TRUE(render_text->style_ranges().empty());
}

TEST_F(RenderTextTest, CustomDefaultStyle) {
  // Test a custom default style.
  scoped_ptr<RenderText> render_text(RenderText::CreateRenderText());
  StyleRange color;
  color.foreground = SK_ColorRED;
  render_text->set_default_style(color);
  render_text->SetText(ASCIIToUTF16("abc"));
  EXPECT_EQ(1U, render_text->style_ranges().size());
  EXPECT_EQ(color.foreground, render_text->style_ranges()[0].foreground);

  // Test that the custom default style persists across clearing text.
  render_text->SetText(string16());
  EXPECT_TRUE(render_text->style_ranges().empty());
  render_text->SetText(ASCIIToUTF16("abc"));
  EXPECT_EQ(1U, render_text->style_ranges().size());
  EXPECT_EQ(color.foreground, render_text->style_ranges()[0].foreground);

  // Test ApplyDefaultStyle after setting a new default.
  StyleRange strike;
  strike.strike = true;
  render_text->set_default_style(strike);
  render_text->ApplyDefaultStyle();
  EXPECT_EQ(1U, render_text->style_ranges().size());
  EXPECT_EQ(true, render_text->style_ranges()[0].strike);
  EXPECT_EQ(strike.foreground, render_text->style_ranges()[0].foreground);
}

TEST_F(RenderTextTest, ApplyStyleRange) {
  scoped_ptr<RenderText> render_text(RenderText::CreateRenderText());
  render_text->SetText(ASCIIToUTF16("01234"));
  EXPECT_EQ(1U, render_text->style_ranges().size());

  // Test ApplyStyleRange (no-op on empty range).
  StyleRange empty;
  empty.range = ui::Range(1, 1);
  render_text->ApplyStyleRange(empty);
  EXPECT_EQ(1U, render_text->style_ranges().size());

  // Test ApplyStyleRange (no-op on invalid range).
  StyleRange invalid;
  invalid.range = ui::Range::InvalidRange();
  render_text->ApplyStyleRange(invalid);
  EXPECT_EQ(1U, render_text->style_ranges().size());

  // Apply a style with a range contained by an existing range.
  StyleRange underline;
  underline.underline = true;
  underline.range = ui::Range(2, 3);
  render_text->ApplyStyleRange(underline);
  EXPECT_EQ(3U, render_text->style_ranges().size());
  EXPECT_EQ(ui::Range(0, 2), render_text->style_ranges()[0].range);
  EXPECT_FALSE(render_text->style_ranges()[0].underline);
  EXPECT_EQ(ui::Range(2, 3), render_text->style_ranges()[1].range);
  EXPECT_TRUE(render_text->style_ranges()[1].underline);
  EXPECT_EQ(ui::Range(3, 5), render_text->style_ranges()[2].range);
  EXPECT_FALSE(render_text->style_ranges()[2].underline);

  // Apply a style with a range equal to another range.
  StyleRange color;
  color.foreground = SK_ColorWHITE;
  color.range = ui::Range(2, 3);
  render_text->ApplyStyleRange(color);
  EXPECT_EQ(3U, render_text->style_ranges().size());
  EXPECT_EQ(ui::Range(0, 2), render_text->style_ranges()[0].range);
  EXPECT_NE(SK_ColorWHITE, render_text->style_ranges()[0].foreground);
  EXPECT_FALSE(render_text->style_ranges()[0].underline);
  EXPECT_EQ(ui::Range(2, 3), render_text->style_ranges()[1].range);
  EXPECT_EQ(SK_ColorWHITE, render_text->style_ranges()[1].foreground);
  EXPECT_FALSE(render_text->style_ranges()[1].underline);
  EXPECT_EQ(ui::Range(3, 5), render_text->style_ranges()[2].range);
  EXPECT_NE(SK_ColorWHITE, render_text->style_ranges()[2].foreground);
  EXPECT_FALSE(render_text->style_ranges()[2].underline);

  // Apply a style with a range containing an existing range.
  // This new style also overlaps portions of neighboring ranges.
  StyleRange strike;
  strike.strike = true;
  strike.range = ui::Range(1, 4);
  render_text->ApplyStyleRange(strike);
  EXPECT_EQ(3U, render_text->style_ranges().size());
  EXPECT_EQ(ui::Range(0, 1), render_text->style_ranges()[0].range);
  EXPECT_FALSE(render_text->style_ranges()[0].strike);
  EXPECT_EQ(ui::Range(1, 4), render_text->style_ranges()[1].range);
  EXPECT_TRUE(render_text->style_ranges()[1].strike);
  EXPECT_EQ(ui::Range(4, 5), render_text->style_ranges()[2].range);
  EXPECT_FALSE(render_text->style_ranges()[2].strike);

  // Apply a style overlapping all ranges.
  StyleRange strike_underline;
  strike_underline.strike = true;
  strike_underline.underline = true;
  strike_underline.range = ui::Range(0, render_text->text().length());
  render_text->ApplyStyleRange(strike_underline);
  EXPECT_EQ(1U, render_text->style_ranges().size());
  EXPECT_EQ(ui::Range(0, 5), render_text->style_ranges()[0].range);
  EXPECT_TRUE(render_text->style_ranges()[0].underline);
  EXPECT_TRUE(render_text->style_ranges()[0].strike);

  // Apply the default style.
  render_text->ApplyDefaultStyle();
  EXPECT_EQ(1U, render_text->style_ranges().size());
  EXPECT_EQ(ui::Range(0, 5), render_text->style_ranges()[0].range);
  EXPECT_FALSE(render_text->style_ranges()[0].underline);
  EXPECT_FALSE(render_text->style_ranges()[0].strike);
}

TEST_F(RenderTextTest, StyleRangesAdjust) {
  // Test that style ranges adjust to the text size.
  scoped_ptr<RenderText> render_text(RenderText::CreateRenderText());
  render_text->SetText(ASCIIToUTF16("abcdef"));
  EXPECT_EQ(1U, render_text->style_ranges().size());
  EXPECT_EQ(ui::Range(0, 6), render_text->style_ranges()[0].range);

  // Test that the range is clipped to the length of shorter text.
  render_text->SetText(ASCIIToUTF16("abc"));
  EXPECT_EQ(1U, render_text->style_ranges().size());
  EXPECT_EQ(ui::Range(0, 3), render_text->style_ranges()[0].range);

  // Test that the last range extends to the length of longer text.
  StyleRange strike;
  strike.strike = true;
  strike.range = ui::Range(2, 3);
  render_text->ApplyStyleRange(strike);
  render_text->SetText(ASCIIToUTF16("abcdefghi"));
  EXPECT_EQ(2U, render_text->style_ranges().size());
  EXPECT_EQ(ui::Range(0, 2), render_text->style_ranges()[0].range);
  EXPECT_EQ(ui::Range(2, 9), render_text->style_ranges()[1].range);
  EXPECT_TRUE(render_text->style_ranges()[1].strike);

  // Test that ranges are removed if they're outside the range of shorter text.
  render_text->SetText(ASCIIToUTF16("ab"));
  EXPECT_EQ(1U, render_text->style_ranges().size());
  EXPECT_EQ(ui::Range(0, 2), render_text->style_ranges()[0].range);
  EXPECT_FALSE(render_text->style_ranges()[0].strike);

  // Test that previously removed ranges don't return.
  render_text->SetText(ASCIIToUTF16("abcdef"));
  EXPECT_EQ(1U, render_text->style_ranges().size());
  EXPECT_EQ(ui::Range(0, 6), render_text->style_ranges()[0].range);
  EXPECT_FALSE(render_text->style_ranges()[0].strike);
}

void RunMoveCursorLeftRightTest(RenderText* render_text,
    const std::vector<SelectionModel>& expected,
    bool move_right) {
  for (int i = 0; i < static_cast<int>(expected.size()); ++i) {
    SelectionModel sel = expected[i];
    EXPECT_TRUE(render_text->selection_model().Equals(sel));
    if (move_right)
      render_text->MoveCursorRight(CHARACTER_BREAK, false);
    else
      render_text->MoveCursorLeft(CHARACTER_BREAK, false);
  }

  SelectionModel sel = expected[expected.size() - 1];
  if (move_right)
    render_text->MoveCursorRight(LINE_BREAK, false);
  else
    render_text->MoveCursorLeft(LINE_BREAK, false);
  EXPECT_TRUE(render_text->selection_model().Equals(sel));
}

TEST_F(RenderTextTest, MoveCursorLeftRightInLtr) {
  scoped_ptr<RenderText> render_text(RenderText::CreateRenderText());

  // Pure LTR.
  render_text->SetText(ASCIIToUTF16("abc"));
  // |expected| saves the expected SelectionModel when moving cursor from left
  // to right.
  std::vector<SelectionModel> expected;
  expected.push_back(SelectionModel(0, 0, SelectionModel::LEADING));
  expected.push_back(SelectionModel(1, 0, SelectionModel::TRAILING));
  expected.push_back(SelectionModel(2, 1, SelectionModel::TRAILING));
  expected.push_back(SelectionModel(3, 2, SelectionModel::TRAILING));
  // The last element is to test the clamped line ends.
  expected.push_back(SelectionModel(3, 2, SelectionModel::TRAILING));
  RunMoveCursorLeftRightTest(render_text.get(), expected, true);

  expected.clear();
  expected.push_back(SelectionModel(3, 2, SelectionModel::TRAILING));
  expected.push_back(SelectionModel(2, 2, SelectionModel::LEADING));
  expected.push_back(SelectionModel(1, 1, SelectionModel::LEADING));
  expected.push_back(SelectionModel(0, 0, SelectionModel::LEADING));
  expected.push_back(SelectionModel(0, 0, SelectionModel::LEADING));
  RunMoveCursorLeftRightTest(render_text.get(), expected, false);
}

TEST_F(RenderTextTest, MoveCursorLeftRightInLtrRtl) {
  scoped_ptr<RenderText> render_text(RenderText::CreateRenderText());
  // LTR-RTL
  render_text->SetText(WideToUTF16(L"abc\x05d0\x05d1\x05d2"));
  // The last one is the expected END position.
  std::vector<SelectionModel> expected;
  expected.push_back(SelectionModel(0, 0, SelectionModel::LEADING));
  expected.push_back(SelectionModel(1, 0, SelectionModel::TRAILING));
  expected.push_back(SelectionModel(2, 1, SelectionModel::TRAILING));
  expected.push_back(SelectionModel(3, 2, SelectionModel::TRAILING));
  expected.push_back(SelectionModel(5, 5, SelectionModel::LEADING));
  expected.push_back(SelectionModel(4, 4, SelectionModel::LEADING));
  expected.push_back(SelectionModel(3, 3, SelectionModel::LEADING));
  expected.push_back(SelectionModel(6, 3, SelectionModel::LEADING));
  expected.push_back(SelectionModel(6, 3, SelectionModel::LEADING));
  RunMoveCursorLeftRightTest(render_text.get(), expected, true);

  expected.clear();
  expected.push_back(SelectionModel(6, 3, SelectionModel::LEADING));
  expected.push_back(SelectionModel(4, 3, SelectionModel::TRAILING));
  expected.push_back(SelectionModel(5, 4, SelectionModel::TRAILING));
  expected.push_back(SelectionModel(6, 5, SelectionModel::TRAILING));
  expected.push_back(SelectionModel(2, 2, SelectionModel::LEADING));
  expected.push_back(SelectionModel(1, 1, SelectionModel::LEADING));
  expected.push_back(SelectionModel(0, 0, SelectionModel::LEADING));
  expected.push_back(SelectionModel(0, 0, SelectionModel::LEADING));
  RunMoveCursorLeftRightTest(render_text.get(), expected, false);
}

TEST_F(RenderTextTest, MoveCursorLeftRightInLtrRtlLtr) {
  scoped_ptr<RenderText> render_text(RenderText::CreateRenderText());
  // LTR-RTL-LTR.
  render_text->SetText(WideToUTF16(L"a"L"\x05d1"L"b"));
  std::vector<SelectionModel> expected;
  expected.push_back(SelectionModel(0, 0, SelectionModel::LEADING));
  expected.push_back(SelectionModel(1, 0, SelectionModel::TRAILING));
  expected.push_back(SelectionModel(1, 1, SelectionModel::LEADING));
  expected.push_back(SelectionModel(3, 2, SelectionModel::TRAILING));
  expected.push_back(SelectionModel(3, 2, SelectionModel::TRAILING));
  RunMoveCursorLeftRightTest(render_text.get(), expected, true);

  expected.clear();
  expected.push_back(SelectionModel(3, 2, SelectionModel::TRAILING));
  expected.push_back(SelectionModel(2, 2, SelectionModel::LEADING));
  expected.push_back(SelectionModel(2, 1, SelectionModel::TRAILING));
  expected.push_back(SelectionModel(0, 0, SelectionModel::LEADING));
  expected.push_back(SelectionModel(0, 0, SelectionModel::LEADING));
  RunMoveCursorLeftRightTest(render_text.get(), expected, false);
}

TEST_F(RenderTextTest, MoveCursorLeftRightInRtl) {
  scoped_ptr<RenderText> render_text(RenderText::CreateRenderText());
  // Pure RTL.
  render_text->SetText(WideToUTF16(L"\x05d0\x05d1\x05d2"));
  render_text->MoveCursorRight(LINE_BREAK, false);
  std::vector<SelectionModel> expected;

#if defined(OS_LINUX)
  expected.push_back(SelectionModel(0, 0, SelectionModel::LEADING));
#else
  expected.push_back(SelectionModel(3, 0, SelectionModel::LEADING));
#endif
  expected.push_back(SelectionModel(1, 0, SelectionModel::TRAILING));
  expected.push_back(SelectionModel(2, 1, SelectionModel::TRAILING));
  expected.push_back(SelectionModel(3, 2, SelectionModel::TRAILING));
#if defined(OS_LINUX)
  expected.push_back(SelectionModel(3, 2, SelectionModel::TRAILING));
#else
  expected.push_back(SelectionModel(0, 2, SelectionModel::TRAILING));
  // TODO(xji): expected (0, 2, TRAILING), actual (3, 0, LEADING).
  // cursor moves from leftmost to rightmost.
  // expected.push_back(SelectionModel(0, 2, SelectionModel::TRAILING));
#endif
  RunMoveCursorLeftRightTest(render_text.get(), expected, false);

  expected.clear();

#if defined(OS_LINUX)
  expected.push_back(SelectionModel(3, 2, SelectionModel::TRAILING));
#else
  expected.push_back(SelectionModel(0, 2, SelectionModel::TRAILING));
#endif
  expected.push_back(SelectionModel(2, 2, SelectionModel::LEADING));
  expected.push_back(SelectionModel(1, 1, SelectionModel::LEADING));
  expected.push_back(SelectionModel(0, 0, SelectionModel::LEADING));
#if defined(OS_LINUX)
  expected.push_back(SelectionModel(0, 0, SelectionModel::LEADING));
#else
  expected.push_back(SelectionModel(3, 0, SelectionModel::LEADING));
  expected.push_back(SelectionModel(3, 0, SelectionModel::LEADING));
#endif
  RunMoveCursorLeftRightTest(render_text.get(), expected, true);
}

TEST_F(RenderTextTest, MoveCursorLeftRightInRtlLtr) {
  scoped_ptr<RenderText> render_text(RenderText::CreateRenderText());
  // RTL-LTR
  render_text->SetText(WideToUTF16(L"\x05d0\x05d1\x05d2"L"abc"));
  render_text->MoveCursorRight(LINE_BREAK, false);
  std::vector<SelectionModel> expected;
#if defined(OS_LINUX)
  expected.push_back(SelectionModel(0, 0, SelectionModel::LEADING));
  expected.push_back(SelectionModel(1, 0, SelectionModel::TRAILING));
  expected.push_back(SelectionModel(2, 1, SelectionModel::TRAILING));
  expected.push_back(SelectionModel(3, 2, SelectionModel::TRAILING));
  expected.push_back(SelectionModel(5, 5, SelectionModel::LEADING));
  expected.push_back(SelectionModel(4, 4, SelectionModel::LEADING));
  expected.push_back(SelectionModel(3, 3, SelectionModel::LEADING));
  expected.push_back(SelectionModel(6, 3, SelectionModel::LEADING));
  expected.push_back(SelectionModel(6, 3, SelectionModel::LEADING));
#else
  expected.push_back(SelectionModel(6, 5, SelectionModel::TRAILING));
  expected.push_back(SelectionModel(5, 5, SelectionModel::LEADING));
  expected.push_back(SelectionModel(4, 4, SelectionModel::LEADING));
  expected.push_back(SelectionModel(3, 3, SelectionModel::LEADING));
  expected.push_back(SelectionModel(1, 0, SelectionModel::TRAILING));
  expected.push_back(SelectionModel(2, 1, SelectionModel::TRAILING));
  expected.push_back(SelectionModel(3, 2, SelectionModel::TRAILING));
  expected.push_back(SelectionModel(0, 2, SelectionModel::TRAILING));
  // TODO(xji): expected (0, 2, TRAILING), actual (3, 0, LEADING).
  // cursor moves from leftmost to middle.
  // expected.push_back(SelectionModel(0, 2, SelectionModel::TRAILING));
#endif

  RunMoveCursorLeftRightTest(render_text.get(), expected, false);

  expected.clear();
#if defined(OS_LINUX)
  expected.push_back(SelectionModel(6, 3, SelectionModel::LEADING));
  expected.push_back(SelectionModel(4, 3, SelectionModel::TRAILING));
  expected.push_back(SelectionModel(5, 4, SelectionModel::TRAILING));
  expected.push_back(SelectionModel(6, 5, SelectionModel::TRAILING));
  expected.push_back(SelectionModel(2, 2, SelectionModel::LEADING));
  expected.push_back(SelectionModel(1, 1, SelectionModel::LEADING));
  expected.push_back(SelectionModel(0, 0, SelectionModel::LEADING));
  expected.push_back(SelectionModel(0, 0, SelectionModel::LEADING));
#else
  expected.push_back(SelectionModel(0, 2, SelectionModel::TRAILING));
  expected.push_back(SelectionModel(2, 2, SelectionModel::LEADING));
  expected.push_back(SelectionModel(1, 1, SelectionModel::LEADING));
  expected.push_back(SelectionModel(0, 0, SelectionModel::LEADING));
  expected.push_back(SelectionModel(4, 3, SelectionModel::TRAILING));
  expected.push_back(SelectionModel(5, 4, SelectionModel::TRAILING));
  expected.push_back(SelectionModel(6, 5, SelectionModel::TRAILING));
  expected.push_back(SelectionModel(6, 5, SelectionModel::TRAILING));
#endif
  RunMoveCursorLeftRightTest(render_text.get(), expected, true);
}

TEST_F(RenderTextTest, MoveCursorLeftRightInRtlLtrRtl) {
  scoped_ptr<RenderText> render_text(RenderText::CreateRenderText());
  // RTL-LTR-RTL.
  render_text->SetText(WideToUTF16(L"\x05d0"L"a"L"\x05d1"));
  render_text->MoveCursorRight(LINE_BREAK, false);
  std::vector<SelectionModel> expected;
#if defined(OS_LINUX)
  expected.push_back(SelectionModel(0, 0, SelectionModel::LEADING));
  expected.push_back(SelectionModel(1, 0, SelectionModel::TRAILING));
  expected.push_back(SelectionModel(1, 1, SelectionModel::LEADING));
  expected.push_back(SelectionModel(3, 2, SelectionModel::TRAILING));
  expected.push_back(SelectionModel(3, 2, SelectionModel::TRAILING));
#else
  expected.push_back(SelectionModel(3, 2, SelectionModel::LEADING));
  expected.push_back(SelectionModel(3, 2, SelectionModel::TRAILING));
  expected.push_back(SelectionModel(1, 1, SelectionModel::LEADING));
  expected.push_back(SelectionModel(1, 0, SelectionModel::TRAILING));
  expected.push_back(SelectionModel(0, 0, SelectionModel::TRAILING));
  // TODO(xji): expected (0, 0, TRAILING), actual (2, 1, LEADING).
  // cursor moves from leftmost to middle.
  // expected.push_back(SelectionModel(0, 0, SelectionModel::TRAILING));
#endif
  RunMoveCursorLeftRightTest(render_text.get(), expected, false);

  expected.clear();
#if defined(OS_LINUX)
  expected.push_back(SelectionModel(3, 2, SelectionModel::TRAILING));
  expected.push_back(SelectionModel(2, 2, SelectionModel::LEADING));
  expected.push_back(SelectionModel(2, 1, SelectionModel::TRAILING));
  expected.push_back(SelectionModel(0, 0, SelectionModel::LEADING));
  expected.push_back(SelectionModel(0, 0, SelectionModel::LEADING));
#else
  expected.push_back(SelectionModel(0, 0, SelectionModel::TRAILING));
  expected.push_back(SelectionModel(0, 0, SelectionModel::LEADING));
  expected.push_back(SelectionModel(2, 1, SelectionModel::TRAILING));
  expected.push_back(SelectionModel(2, 2, SelectionModel::LEADING));
  expected.push_back(SelectionModel(3, 2, SelectionModel::LEADING));
  expected.push_back(SelectionModel(3, 2, SelectionModel::LEADING));
#endif
  RunMoveCursorLeftRightTest(render_text.get(), expected, true);
}

// TODO(xji): temporarily disable in platform Win since the complex script
// characters turned into empty square due to font regression. So, not able
// to test 2 characters belong to the same grapheme.
#if defined(OS_LINUX)
TEST_F(RenderTextTest, MoveCursorLeftRight_ComplexScript) {
  scoped_ptr<RenderText> render_text(RenderText::CreateRenderText());

  render_text->SetText(WideToUTF16(L"\x0915\x093f\x0915\x094d\x0915"));
  EXPECT_EQ(0U, render_text->GetCursorPosition());
  render_text->MoveCursorRight(CHARACTER_BREAK, false);
  EXPECT_EQ(2U, render_text->GetCursorPosition());
  render_text->MoveCursorRight(CHARACTER_BREAK, false);
  EXPECT_EQ(4U, render_text->GetCursorPosition());
  render_text->MoveCursorRight(CHARACTER_BREAK, false);
  EXPECT_EQ(5U, render_text->GetCursorPosition());
  render_text->MoveCursorRight(CHARACTER_BREAK, false);
  EXPECT_EQ(5U, render_text->GetCursorPosition());

  render_text->MoveCursorLeft(CHARACTER_BREAK, false);
  EXPECT_EQ(4U, render_text->GetCursorPosition());
  render_text->MoveCursorLeft(CHARACTER_BREAK, false);
  EXPECT_EQ(2U, render_text->GetCursorPosition());
  render_text->MoveCursorLeft(CHARACTER_BREAK, false);
  EXPECT_EQ(0U, render_text->GetCursorPosition());
  render_text->MoveCursorLeft(CHARACTER_BREAK, false);
  EXPECT_EQ(0U, render_text->GetCursorPosition());
}
#endif

TEST_F(RenderTextTest, MoveCursorLeftRightWithSelection) {
  scoped_ptr<RenderText> render_text(RenderText::CreateRenderText());
  render_text->SetText(WideToUTF16(L"abc\x05d0\x05d1\x05d2"));
  // Left arrow on select ranging (6, 4).
  render_text->MoveCursorRight(LINE_BREAK, false);
  EXPECT_EQ(6U, render_text->GetCursorPosition());
  render_text->MoveCursorLeft(CHARACTER_BREAK, false);
  EXPECT_EQ(4U, render_text->GetCursorPosition());
  render_text->MoveCursorLeft(CHARACTER_BREAK, false);
  EXPECT_EQ(5U, render_text->GetCursorPosition());
  render_text->MoveCursorLeft(CHARACTER_BREAK, false);
  EXPECT_EQ(6U, render_text->GetCursorPosition());
  render_text->MoveCursorRight(CHARACTER_BREAK, true);
  EXPECT_EQ(6U, render_text->GetSelectionStart());
  EXPECT_EQ(5U, render_text->GetCursorPosition());
  render_text->MoveCursorRight(CHARACTER_BREAK, true);
  EXPECT_EQ(6U, render_text->GetSelectionStart());
  EXPECT_EQ(4U, render_text->GetCursorPosition());
  render_text->MoveCursorLeft(CHARACTER_BREAK, false);
  EXPECT_EQ(6U, render_text->GetCursorPosition());

  // Right arrow on select ranging (4, 6).
  render_text->MoveCursorLeft(LINE_BREAK, false);
  EXPECT_EQ(0U, render_text->GetCursorPosition());
  render_text->MoveCursorRight(CHARACTER_BREAK, false);
  EXPECT_EQ(1U, render_text->GetCursorPosition());
  render_text->MoveCursorRight(CHARACTER_BREAK, false);
  EXPECT_EQ(2U, render_text->GetCursorPosition());
  render_text->MoveCursorRight(CHARACTER_BREAK, false);
  EXPECT_EQ(3U, render_text->GetCursorPosition());
  render_text->MoveCursorRight(CHARACTER_BREAK, false);
  EXPECT_EQ(5U, render_text->GetCursorPosition());
  render_text->MoveCursorRight(CHARACTER_BREAK, false);
  EXPECT_EQ(4U, render_text->GetCursorPosition());
  render_text->MoveCursorLeft(CHARACTER_BREAK, true);
  EXPECT_EQ(4U, render_text->GetSelectionStart());
  EXPECT_EQ(5U, render_text->GetCursorPosition());
  render_text->MoveCursorLeft(CHARACTER_BREAK, true);
  EXPECT_EQ(4U, render_text->GetSelectionStart());
  EXPECT_EQ(6U, render_text->GetCursorPosition());
  render_text->MoveCursorRight(CHARACTER_BREAK, false);
  EXPECT_EQ(4U, render_text->GetCursorPosition());
}

}  // namespace gfx
