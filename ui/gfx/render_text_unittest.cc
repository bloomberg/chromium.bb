// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/render_text.h"

#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace gfx {

namespace {

// Checks whether |range| contains |index|. This is not the same as calling
// |range.Contains(ui::Range(index))| - as that would return true when
// |index| == |range.end()|.
bool IndexInRange(const ui::Range& range, size_t index) {
  return index >= range.start() && index < range.end();
}

}  // namespace

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

  // Apply new style range that contains the 2nd last old style range.
  render_text->SetText(ASCIIToUTF16("abcdefghi"));
  underline.range = ui::Range(0, 3);
  render_text->ApplyStyleRange(underline);
  color.range = ui::Range(3, 6);
  render_text->ApplyStyleRange(color);
  strike.range = ui::Range(6, 9);
  render_text->ApplyStyleRange(strike);
  EXPECT_EQ(3U, render_text->style_ranges().size());

  color.foreground = SK_ColorRED;
  color.range = ui::Range(2, 8);
  render_text->ApplyStyleRange(color);
  EXPECT_EQ(3U, render_text->style_ranges().size());
  EXPECT_EQ(ui::Range(0, 2), render_text->style_ranges()[0].range);
  EXPECT_TRUE(render_text->style_ranges()[0].underline);
  EXPECT_EQ(ui::Range(2, 8), render_text->style_ranges()[1].range);
  EXPECT_EQ(SK_ColorRED, render_text->style_ranges()[1].foreground);
  EXPECT_EQ(ui::Range(8, 9), render_text->style_ranges()[2].range);
  EXPECT_TRUE(render_text->style_ranges()[2].strike);

  // Apply new style range that contains multiple old style ranges.
  render_text->SetText(ASCIIToUTF16("abcdefghiopq"));
  underline.range = ui::Range(0, 3);
  render_text->ApplyStyleRange(underline);
  color.range = ui::Range(3, 6);
  render_text->ApplyStyleRange(color);
  strike.range = ui::Range(6, 9);
  render_text->ApplyStyleRange(strike);
  strike_underline.range = ui::Range(9, 12);
  render_text->ApplyStyleRange(strike_underline);
  EXPECT_EQ(4U, render_text->style_ranges().size());

  color.foreground = SK_ColorRED;
  color.range = ui::Range(2, 10);
  render_text->ApplyStyleRange(color);
  EXPECT_EQ(3U, render_text->style_ranges().size());
  EXPECT_EQ(ui::Range(0, 2), render_text->style_ranges()[0].range);
  EXPECT_TRUE(render_text->style_ranges()[0].underline);
  EXPECT_EQ(ui::Range(2, 10), render_text->style_ranges()[1].range);
  EXPECT_EQ(SK_ColorRED, render_text->style_ranges()[1].foreground);
  EXPECT_EQ(ui::Range(10, 12), render_text->style_ranges()[2].range);
  EXPECT_TRUE(render_text->style_ranges()[2].underline);
  EXPECT_TRUE(render_text->style_ranges()[2].strike);
}

static void SetTextWith2ExtraStyles(RenderText* render_text) {
  render_text->SetText(ASCIIToUTF16("abcdefghi"));

  gfx::StyleRange strike;
  strike.strike = true;
  strike.range = ui::Range(0, 3);
  render_text->ApplyStyleRange(strike);

  gfx::StyleRange underline;
  underline.underline = true;
  underline.range = ui::Range(3, 6);
  render_text->ApplyStyleRange(underline);
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

  // Test that ranges are removed correctly if they are outside the range of
  // shorter text.
  SetTextWith2ExtraStyles(render_text.get());
  EXPECT_EQ(3U, render_text->style_ranges().size());

  render_text->SetText(ASCIIToUTF16("abcdefg"));
  EXPECT_EQ(3U, render_text->style_ranges().size());
  EXPECT_EQ(ui::Range(0, 3), render_text->style_ranges()[0].range);
  EXPECT_EQ(ui::Range(3, 6), render_text->style_ranges()[1].range);
  EXPECT_EQ(ui::Range(6, 7), render_text->style_ranges()[2].range);

  SetTextWith2ExtraStyles(render_text.get());
  EXPECT_EQ(3U, render_text->style_ranges().size());

  render_text->SetText(ASCIIToUTF16("abcdef"));
  EXPECT_EQ(2U, render_text->style_ranges().size());
  EXPECT_EQ(ui::Range(0, 3), render_text->style_ranges()[0].range);
  EXPECT_EQ(ui::Range(3, 6), render_text->style_ranges()[1].range);

  SetTextWith2ExtraStyles(render_text.get());
  EXPECT_EQ(3U, render_text->style_ranges().size());

  render_text->SetText(ASCIIToUTF16("abcde"));
  EXPECT_EQ(2U, render_text->style_ranges().size());
  EXPECT_EQ(ui::Range(0, 3), render_text->style_ranges()[0].range);
  EXPECT_EQ(ui::Range(3, 5), render_text->style_ranges()[1].range);

  SetTextWith2ExtraStyles(render_text.get());
  EXPECT_EQ(3U, render_text->style_ranges().size());

  render_text->SetText(ASCIIToUTF16("abc"));
  EXPECT_EQ(1U, render_text->style_ranges().size());
  EXPECT_EQ(ui::Range(0, 3), render_text->style_ranges()[0].range);

  SetTextWith2ExtraStyles(render_text.get());
  EXPECT_EQ(3U, render_text->style_ranges().size());

  render_text->SetText(ASCIIToUTF16("a"));
  EXPECT_EQ(1U, render_text->style_ranges().size());
  EXPECT_EQ(ui::Range(0, 1), render_text->style_ranges()[0].range);
}

void TestVisualCursorMotionInObscuredField(RenderText* render_text,
                                           const string16& text,
                                           bool select) {
  render_text->SetText(text);
  int len = text.length();
  render_text->MoveCursor(LINE_BREAK, CURSOR_RIGHT, select);
  EXPECT_EQ(SelectionModel(ui::Range(select ? 0 : len, len), CURSOR_FORWARD),
            render_text->selection_model());
  render_text->MoveCursor(LINE_BREAK, CURSOR_LEFT, select);
  EXPECT_EQ(SelectionModel(0, CURSOR_BACKWARD), render_text->selection_model());
  for (int j = 1; j <= len; ++j) {
    render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, select);
    EXPECT_EQ(SelectionModel(ui::Range(select ? 0 : j, j), CURSOR_BACKWARD),
              render_text->selection_model());
  }
  for (int j = len - 1; j >= 0; --j) {
    render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, select);
    EXPECT_EQ(SelectionModel(ui::Range(select ? 0 : j, j), CURSOR_FORWARD),
              render_text->selection_model());
  }
  render_text->MoveCursor(WORD_BREAK, CURSOR_RIGHT, select);
  EXPECT_EQ(SelectionModel(ui::Range(select ? 0 : len, len), CURSOR_FORWARD),
            render_text->selection_model());
  render_text->MoveCursor(WORD_BREAK, CURSOR_LEFT, select);
  EXPECT_EQ(SelectionModel(0, CURSOR_BACKWARD), render_text->selection_model());
}

TEST_F(RenderTextTest, PasswordCensorship) {
  const string16 seuss = ASCIIToUTF16("hop on pop");
  const string16 no_seuss = ASCIIToUTF16("**********");
  scoped_ptr<RenderText> render_text(RenderText::CreateRenderText());

  // GetObscuredText returns asterisks when the obscured bit is set.
  render_text->SetText(seuss);
  render_text->SetObscured(true);
  EXPECT_EQ(seuss, render_text->text());
  EXPECT_EQ(no_seuss, render_text->GetDisplayText());
  render_text->SetObscured(false);
  EXPECT_EQ(seuss, render_text->text());
  EXPECT_EQ(seuss, render_text->GetDisplayText());

// TODO(benrg): No Windows implementation yet.
#if !defined(OS_WIN)

  render_text->SetObscured(true);

  // Surrogate pairs are counted as one code point.
  const char16 invalid_surrogates[] = {0xDC00, 0xD800, 0};
  render_text->SetText(invalid_surrogates);
  EXPECT_EQ(ASCIIToUTF16("**"), render_text->GetDisplayText());
  const char16 valid_surrogates[] = {0xD800, 0xDC00, 0};
  render_text->SetText(valid_surrogates);
  EXPECT_EQ(ASCIIToUTF16("*"), render_text->GetDisplayText());
  EXPECT_EQ(0U, render_text->cursor_position());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, false);
  EXPECT_EQ(2U, render_text->cursor_position());

  // Cursoring is independent of the underlying characters when the text is
  // obscured.
  const wchar_t* const texts[] = {
    L"hop on pop",           // word boundaries
    L"ab \x5D0\x5D1" L"12",  // bidi embedding level of 2
    L"\x5D0\x5D1" L"12",     // RTL paragraph direction on Linux
    L"\x5D0\x5D1"            // pure RTL
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(texts); ++i) {
    string16 text = WideToUTF16(texts[i]);
    TestVisualCursorMotionInObscuredField(render_text.get(), text, false);
    TestVisualCursorMotionInObscuredField(render_text.get(), text, true);
  }
#endif  // !defined(OS_WIN)
}

void RunMoveCursorLeftRightTest(RenderText* render_text,
    const std::vector<SelectionModel>& expected,
    VisualCursorDirection direction) {
  for (size_t i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(expected[i], render_text->selection_model());
    render_text->MoveCursor(CHARACTER_BREAK, direction, false);
  }
  // Check that cursoring is clamped at the line edge.
  EXPECT_EQ(expected.back(), render_text->selection_model());
  // Check that it is the line edge.
  render_text->MoveCursor(LINE_BREAK, direction, false);
  EXPECT_EQ(expected.back(), render_text->selection_model());
}

TEST_F(RenderTextTest, MoveCursorLeftRightInLtr) {
  scoped_ptr<RenderText> render_text(RenderText::CreateRenderText());

  // Pure LTR.
  render_text->SetText(ASCIIToUTF16("abc"));
  // |expected| saves the expected SelectionModel when moving cursor from left
  // to right.
  std::vector<SelectionModel> expected;
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(1, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(2, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(3, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(3, CURSOR_FORWARD));
  RunMoveCursorLeftRightTest(render_text.get(), expected, CURSOR_RIGHT);

  expected.clear();
  expected.push_back(SelectionModel(3, CURSOR_FORWARD));
  expected.push_back(SelectionModel(2, CURSOR_FORWARD));
  expected.push_back(SelectionModel(1, CURSOR_FORWARD));
  expected.push_back(SelectionModel(0, CURSOR_FORWARD));
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
  RunMoveCursorLeftRightTest(render_text.get(), expected, CURSOR_LEFT);
}

TEST_F(RenderTextTest, MoveCursorLeftRightInLtrRtl) {
  scoped_ptr<RenderText> render_text(RenderText::CreateRenderText());
  // LTR-RTL
  render_text->SetText(WideToUTF16(L"abc\x05d0\x05d1\x05d2"));
  // The last one is the expected END position.
  std::vector<SelectionModel> expected;
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(1, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(2, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(3, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(5, CURSOR_FORWARD));
  expected.push_back(SelectionModel(4, CURSOR_FORWARD));
  expected.push_back(SelectionModel(3, CURSOR_FORWARD));
  expected.push_back(SelectionModel(6, CURSOR_FORWARD));
  RunMoveCursorLeftRightTest(render_text.get(), expected, CURSOR_RIGHT);

  expected.clear();
  expected.push_back(SelectionModel(6, CURSOR_FORWARD));
  expected.push_back(SelectionModel(4, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(5, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(6, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(2, CURSOR_FORWARD));
  expected.push_back(SelectionModel(1, CURSOR_FORWARD));
  expected.push_back(SelectionModel(0, CURSOR_FORWARD));
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
  RunMoveCursorLeftRightTest(render_text.get(), expected, CURSOR_LEFT);
}

TEST_F(RenderTextTest, MoveCursorLeftRightInLtrRtlLtr) {
  scoped_ptr<RenderText> render_text(RenderText::CreateRenderText());
  // LTR-RTL-LTR.
  render_text->SetText(WideToUTF16(L"a"L"\x05d1"L"b"));
  std::vector<SelectionModel> expected;
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(1, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(1, CURSOR_FORWARD));
  expected.push_back(SelectionModel(3, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(3, CURSOR_FORWARD));
  RunMoveCursorLeftRightTest(render_text.get(), expected, CURSOR_RIGHT);

  expected.clear();
  expected.push_back(SelectionModel(3, CURSOR_FORWARD));
  expected.push_back(SelectionModel(2, CURSOR_FORWARD));
  expected.push_back(SelectionModel(2, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(0, CURSOR_FORWARD));
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
  RunMoveCursorLeftRightTest(render_text.get(), expected, CURSOR_LEFT);
}

TEST_F(RenderTextTest, MoveCursorLeftRightInRtl) {
  scoped_ptr<RenderText> render_text(RenderText::CreateRenderText());
  // Pure RTL.
  render_text->SetText(WideToUTF16(L"\x05d0\x05d1\x05d2"));
  render_text->MoveCursor(LINE_BREAK, CURSOR_RIGHT, false);
  std::vector<SelectionModel> expected;

#if defined(OS_LINUX)
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
#else
  expected.push_back(SelectionModel(3, CURSOR_FORWARD));
#endif
  expected.push_back(SelectionModel(1, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(2, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(3, CURSOR_BACKWARD));
#if defined(OS_LINUX)
  expected.push_back(SelectionModel(3, CURSOR_FORWARD));
#else
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
#endif
  RunMoveCursorLeftRightTest(render_text.get(), expected, CURSOR_LEFT);

  expected.clear();

#if defined(OS_LINUX)
  expected.push_back(SelectionModel(3, CURSOR_FORWARD));
#else
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
#endif
  expected.push_back(SelectionModel(2, CURSOR_FORWARD));
  expected.push_back(SelectionModel(1, CURSOR_FORWARD));
  expected.push_back(SelectionModel(0, CURSOR_FORWARD));
#if defined(OS_LINUX)
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
#else
  expected.push_back(SelectionModel(3, CURSOR_FORWARD));
#endif
  RunMoveCursorLeftRightTest(render_text.get(), expected, CURSOR_RIGHT);
}

TEST_F(RenderTextTest, MoveCursorLeftRightInRtlLtr) {
  scoped_ptr<RenderText> render_text(RenderText::CreateRenderText());
  // RTL-LTR
  render_text->SetText(WideToUTF16(L"\x05d0\x05d1\x05d2"L"abc"));
  render_text->MoveCursor(LINE_BREAK, CURSOR_RIGHT, false);
  std::vector<SelectionModel> expected;
#if defined(OS_LINUX)
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(1, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(2, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(3, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(5, CURSOR_FORWARD));
  expected.push_back(SelectionModel(4, CURSOR_FORWARD));
  expected.push_back(SelectionModel(3, CURSOR_FORWARD));
  expected.push_back(SelectionModel(6, CURSOR_FORWARD));
#else
  expected.push_back(SelectionModel(6, CURSOR_FORWARD));
  expected.push_back(SelectionModel(5, CURSOR_FORWARD));
  expected.push_back(SelectionModel(4, CURSOR_FORWARD));
  expected.push_back(SelectionModel(3, CURSOR_FORWARD));
  expected.push_back(SelectionModel(1, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(2, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(3, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
#endif

  RunMoveCursorLeftRightTest(render_text.get(), expected, CURSOR_LEFT);

  expected.clear();
#if defined(OS_LINUX)
  expected.push_back(SelectionModel(6, CURSOR_FORWARD));
  expected.push_back(SelectionModel(4, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(5, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(6, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(2, CURSOR_FORWARD));
  expected.push_back(SelectionModel(1, CURSOR_FORWARD));
  expected.push_back(SelectionModel(0, CURSOR_FORWARD));
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
#else
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(2, CURSOR_FORWARD));
  expected.push_back(SelectionModel(1, CURSOR_FORWARD));
  expected.push_back(SelectionModel(0, CURSOR_FORWARD));
  expected.push_back(SelectionModel(4, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(5, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(6, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(6, CURSOR_FORWARD));
#endif
  RunMoveCursorLeftRightTest(render_text.get(), expected, CURSOR_RIGHT);
}

TEST_F(RenderTextTest, MoveCursorLeftRightInRtlLtrRtl) {
  scoped_ptr<RenderText> render_text(RenderText::CreateRenderText());
  // RTL-LTR-RTL.
  render_text->SetText(WideToUTF16(L"\x05d0"L"a"L"\x05d1"));
  render_text->MoveCursor(LINE_BREAK, CURSOR_RIGHT, false);
  std::vector<SelectionModel> expected;
#if defined(OS_LINUX)
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(1, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(1, CURSOR_FORWARD));
  expected.push_back(SelectionModel(3, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(3, CURSOR_FORWARD));
#else
  expected.push_back(SelectionModel(3, CURSOR_FORWARD));
  expected.push_back(SelectionModel(3, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(1, CURSOR_FORWARD));
  expected.push_back(SelectionModel(1, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
#endif
  RunMoveCursorLeftRightTest(render_text.get(), expected, CURSOR_LEFT);

  expected.clear();
#if defined(OS_LINUX)
  expected.push_back(SelectionModel(3, CURSOR_FORWARD));
  expected.push_back(SelectionModel(2, CURSOR_FORWARD));
  expected.push_back(SelectionModel(2, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(0, CURSOR_FORWARD));
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
#else
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(0, CURSOR_FORWARD));
  expected.push_back(SelectionModel(2, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(2, CURSOR_FORWARD));
  expected.push_back(SelectionModel(3, CURSOR_FORWARD));
#endif
  RunMoveCursorLeftRightTest(render_text.get(), expected, CURSOR_RIGHT);
}

// TODO(xji): temporarily disable in platform Win since the complex script
// characters turned into empty square due to font regression. So, not able
// to test 2 characters belong to the same grapheme.
#if defined(OS_LINUX)
TEST_F(RenderTextTest, MoveCursorLeftRight_ComplexScript) {
  scoped_ptr<RenderText> render_text(RenderText::CreateRenderText());

  render_text->SetText(WideToUTF16(L"\x0915\x093f\x0915\x094d\x0915"));
  EXPECT_EQ(0U, render_text->cursor_position());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, false);
  EXPECT_EQ(2U, render_text->cursor_position());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, false);
  EXPECT_EQ(4U, render_text->cursor_position());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, false);
  EXPECT_EQ(5U, render_text->cursor_position());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, false);
  EXPECT_EQ(5U, render_text->cursor_position());

  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, false);
  EXPECT_EQ(4U, render_text->cursor_position());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, false);
  EXPECT_EQ(2U, render_text->cursor_position());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, false);
  EXPECT_EQ(0U, render_text->cursor_position());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, false);
  EXPECT_EQ(0U, render_text->cursor_position());
}
#endif

TEST_F(RenderTextTest, GraphemePositions) {
  // LTR 2-character grapheme, LTR abc, LTR 2-character grapheme.
  const string16 kText1 = WideToUTF16(L"\x0915\x093f"L"abc"L"\x0915\x093f");

  // LTR ab, LTR 2-character grapheme, LTR cd.
  const string16 kText2 = WideToUTF16(L"ab"L"\x0915\x093f"L"cd");

  struct {
    string16 text;
    size_t index;
    size_t expected_previous;
    size_t expected_next;
  } cases[] = {
    { string16(), 0, 0, 0 },
    { string16(), 1, 0, 0 },
    { string16(), 50, 0, 0 },
    { kText1, 0, 0, 2 },
    { kText1, 1, 0, 2 },
    { kText1, 2, 0, 3 },
    { kText1, 3, 2, 4 },
    { kText1, 4, 3, 5 },
    { kText1, 5, 4, 7 },
    { kText1, 6, 5, 7 },
    { kText1, 7, 5, 7 },
    { kText1, 8, 7, 7 },
    { kText1, 50, 7, 7 },
    { kText2, 0, 0, 1 },
    { kText2, 1, 0, 2 },
    { kText2, 2, 1, 4 },
    { kText2, 3, 2, 4 },
    { kText2, 4, 2, 5 },
    { kText2, 5, 4, 6 },
    { kText2, 6, 5, 6 },
    { kText2, 7, 6, 6 },
    { kText2, 50, 6, 6 },
  };

  // TODO(asvitkine): Disable tests that fail on XP bots due to lack of complete
  //                  font support for some scripts - http://crbug.com/106450
#if defined(OS_WIN)
  if (base::win::GetVersion() < base::win::VERSION_VISTA)
    return;
#endif

  scoped_ptr<RenderText> render_text(RenderText::CreateRenderText());
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); i++) {
    render_text->SetText(cases[i].text);

    size_t next = render_text->IndexOfAdjacentGrapheme(cases[i].index,
                                                       CURSOR_FORWARD);
    EXPECT_EQ(cases[i].expected_next, next);
    EXPECT_TRUE(render_text->IsCursorablePosition(next));

    size_t previous = render_text->IndexOfAdjacentGrapheme(cases[i].index,
                                                           CURSOR_BACKWARD);
    EXPECT_EQ(cases[i].expected_previous, previous);
    EXPECT_TRUE(render_text->IsCursorablePosition(previous));
  }
}

TEST_F(RenderTextTest, EdgeSelectionModels) {
  // Simple Latin text.
  const string16 kLatin = WideToUTF16(L"abc");
  // LTR 2-character grapheme.
  const string16 kLTRGrapheme = WideToUTF16(L"\x0915\x093f");
  // LTR 2-character grapheme, LTR a, LTR 2-character grapheme.
  const string16 kHindiLatin = WideToUTF16(L"\x0915\x093f"L"a"L"\x0915\x093f");
  // RTL 2-character grapheme.
  const string16 kRTLGrapheme = WideToUTF16(L"\x05e0\x05b8");
  // RTL 2-character grapheme, LTR a, RTL 2-character grapheme.
  const string16 kHebrewLatin = WideToUTF16(L"\x05e0\x05b8"L"a"L"\x05e0\x05b8");

  struct {
    string16 text;
    base::i18n::TextDirection expected_text_direction;
  } cases[] = {
    { string16(),   base::i18n::LEFT_TO_RIGHT },
    { kLatin,       base::i18n::LEFT_TO_RIGHT },
    { kLTRGrapheme, base::i18n::LEFT_TO_RIGHT },
    { kHindiLatin,  base::i18n::LEFT_TO_RIGHT },
#if defined(OS_LINUX)
    // On Linux, the whole string is displayed RTL, rather than individual runs.
    { kRTLGrapheme, base::i18n::RIGHT_TO_LEFT },
    { kHebrewLatin, base::i18n::RIGHT_TO_LEFT },
#else
    { kRTLGrapheme, base::i18n::LEFT_TO_RIGHT },
    { kHebrewLatin, base::i18n::LEFT_TO_RIGHT },
#endif
  };

  // TODO(asvitkine): Disable tests that fail on XP bots due to lack of complete
  //                  font support for some scripts - http://crbug.com/106450
#if defined(OS_WIN)
  if (base::win::GetVersion() < base::win::VERSION_VISTA)
    return;
#endif

  scoped_ptr<RenderText> render_text(RenderText::CreateRenderText());
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); i++) {
    render_text->SetText(cases[i].text);
    bool ltr = (cases[i].expected_text_direction == base::i18n::LEFT_TO_RIGHT);

    SelectionModel start_edge =
        render_text->EdgeSelectionModel(ltr ? CURSOR_LEFT : CURSOR_RIGHT);
    EXPECT_EQ(start_edge, SelectionModel(0, CURSOR_BACKWARD));

    SelectionModel end_edge =
        render_text->EdgeSelectionModel(ltr ? CURSOR_RIGHT : CURSOR_LEFT);
    EXPECT_EQ(end_edge, SelectionModel(cases[i].text.length(), CURSOR_FORWARD));
  }
}

TEST_F(RenderTextTest, MoveCursorLeftRightWithSelection) {
  scoped_ptr<RenderText> render_text(RenderText::CreateRenderText());
  render_text->SetText(WideToUTF16(L"abc\x05d0\x05d1\x05d2"));
  // Left arrow on select ranging (6, 4).
  render_text->MoveCursor(LINE_BREAK, CURSOR_RIGHT, false);
  EXPECT_EQ(ui::Range(6), render_text->selection());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, false);
  EXPECT_EQ(ui::Range(4), render_text->selection());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, false);
  EXPECT_EQ(ui::Range(5), render_text->selection());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, false);
  EXPECT_EQ(ui::Range(6), render_text->selection());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, true);
  EXPECT_EQ(ui::Range(6, 5), render_text->selection());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, true);
  EXPECT_EQ(ui::Range(6, 4), render_text->selection());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, false);
  EXPECT_EQ(ui::Range(6), render_text->selection());

  // Right arrow on select ranging (4, 6).
  render_text->MoveCursor(LINE_BREAK, CURSOR_LEFT, false);
  EXPECT_EQ(ui::Range(0), render_text->selection());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, false);
  EXPECT_EQ(ui::Range(1), render_text->selection());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, false);
  EXPECT_EQ(ui::Range(2), render_text->selection());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, false);
  EXPECT_EQ(ui::Range(3), render_text->selection());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, false);
  EXPECT_EQ(ui::Range(5), render_text->selection());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, false);
  EXPECT_EQ(ui::Range(4), render_text->selection());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, true);
  EXPECT_EQ(ui::Range(4, 5), render_text->selection());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, true);
  EXPECT_EQ(ui::Range(4, 6), render_text->selection());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, false);
  EXPECT_EQ(ui::Range(4), render_text->selection());
}

// TODO(xji): Make these work on Windows.
#if defined(OS_LINUX)
void MoveLeftRightByWordVerifier(RenderText* render_text,
                                 const wchar_t* str) {
  render_text->SetText(WideToUTF16(str));

  // Test moving by word from left ro right.
  render_text->MoveCursor(LINE_BREAK, CURSOR_LEFT, false);
  bool first_word = true;
  while (true) {
    // First, test moving by word from a word break position, such as from
    // "|abc def" to "abc| def".
    SelectionModel start = render_text->selection_model();
    render_text->MoveCursor(WORD_BREAK, CURSOR_RIGHT, false);
    SelectionModel end = render_text->selection_model();
    if (end == start)  // reach the end.
      break;

    // For testing simplicity, each word is a 3-character word.
    int num_of_character_moves = first_word ? 3 : 4;
    first_word = false;
    render_text->MoveCursorTo(start);
    for (int j = 0; j < num_of_character_moves; ++j)
      render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, false);
    EXPECT_EQ(end, render_text->selection_model());

    // Then, test moving by word from positions inside the word, such as from
    // "a|bc def" to "abc| def", and from "ab|c def" to "abc| def".
    for (int j = 1; j < num_of_character_moves; ++j) {
      render_text->MoveCursorTo(start);
      for (int k = 0; k < j; ++k)
        render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, false);
      render_text->MoveCursor(WORD_BREAK, CURSOR_RIGHT, false);
      EXPECT_EQ(end, render_text->selection_model());
    }
  }

  // Test moving by word from right to left.
  render_text->MoveCursor(LINE_BREAK, CURSOR_RIGHT, false);
  first_word = true;
  while (true) {
    SelectionModel start = render_text->selection_model();
    render_text->MoveCursor(WORD_BREAK, CURSOR_LEFT, false);
    SelectionModel end = render_text->selection_model();
    if (end == start)  // reach the end.
      break;

    int num_of_character_moves = first_word ? 3 : 4;
    first_word = false;
    render_text->MoveCursorTo(start);
    for (int j = 0; j < num_of_character_moves; ++j)
      render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, false);
    EXPECT_EQ(end, render_text->selection_model());

    for (int j = 1; j < num_of_character_moves; ++j) {
      render_text->MoveCursorTo(start);
      for (int k = 0; k < j; ++k)
        render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, false);
      render_text->MoveCursor(WORD_BREAK, CURSOR_LEFT, false);
      EXPECT_EQ(end, render_text->selection_model());
    }
  }
}

TEST_F(RenderTextTest, MoveLeftRightByWordInBidiText) {
  scoped_ptr<RenderText> render_text(RenderText::CreateRenderText());

  // For testing simplicity, each word is a 3-character word.
  std::vector<const wchar_t*> test;
  test.push_back(L"abc");
  test.push_back(L"abc def");
  test.push_back(L"\x05E1\x05E2\x05E3");
  test.push_back(L"\x05E1\x05E2\x05E3 \x05E4\x05E5\x05E6");
  test.push_back(L"abc \x05E1\x05E2\x05E3");
  test.push_back(L"abc def \x05E1\x05E2\x05E3 \x05E4\x05E5\x05E6");
  test.push_back(L"abc def hij \x05E1\x05E2\x05E3 \x05E4\x05E5\x05E6"
                 L" \x05E7\x05E8\x05E9");

  test.push_back(L"abc \x05E1\x05E2\x05E3 hij");
  test.push_back(L"abc def \x05E1\x05E2\x05E3 \x05E4\x05E5\x05E6 hij opq");
  test.push_back(L"abc def hij \x05E1\x05E2\x05E3 \x05E4\x05E5\x05E6"
                 L" \x05E7\x05E8\x05E9"L" opq rst uvw");

  test.push_back(L"\x05E1\x05E2\x05E3 abc");
  test.push_back(L"\x05E1\x05E2\x05E3 \x05E4\x05E5\x05E6 abc def");
  test.push_back(L"\x05E1\x05E2\x05E3 \x05E4\x05E5\x05E6 \x05E7\x05E8\x05E9"
                 L" abc def hij");

  test.push_back(L"\x05D1\x05D2\x05D3 abc \x05E1\x05E2\x05E3");
  test.push_back(L"\x05D1\x05D2\x05D3 \x05D4\x05D5\x05D6 abc def"
                 L" \x05E1\x05E2\x05E3 \x05E4\x05E5\x05E6");
  test.push_back(L"\x05D1\x05D2\x05D3 \x05D4\x05D5\x05D6 \x05D7\x05D8\x05D9"
                 L" abc def hij \x05E1\x05E2\x05E3 \x05E4\x05E5\x05E6"
                 L" \x05E7\x05E8\x05E9");

  for (size_t i = 0; i < test.size(); ++i)
    MoveLeftRightByWordVerifier(render_text.get(), test[i]);
}

TEST_F(RenderTextTest, MoveLeftRightByWordInBidiText_TestEndOfText) {
  scoped_ptr<RenderText> render_text(RenderText::CreateRenderText());

  render_text->SetText(WideToUTF16(L"ab\x05E1"));
  // Moving the cursor by word from "abC|" to the left should return "|abC".
  // But since end of text is always treated as a word break, it returns
  // position "ab|C".
  // TODO(xji): Need to make it work as expected.
  render_text->MoveCursor(LINE_BREAK, CURSOR_RIGHT, false);
  render_text->MoveCursor(WORD_BREAK, CURSOR_LEFT, false);
  // EXPECT_EQ(SelectionModel(), render_text->selection_model());

  // Moving the cursor by word from "|abC" to the right returns "abC|".
  render_text->MoveCursor(LINE_BREAK, CURSOR_LEFT, false);
  render_text->MoveCursor(WORD_BREAK, CURSOR_RIGHT, false);
  EXPECT_EQ(SelectionModel(3, CURSOR_FORWARD), render_text->selection_model());

  render_text->SetText(WideToUTF16(L"\x05E1\x05E2"L"a"));
  // For logical text "BCa", moving the cursor by word from "aCB|" to the left
  // returns "|aCB".
  render_text->MoveCursor(LINE_BREAK, CURSOR_RIGHT, false);
  render_text->MoveCursor(WORD_BREAK, CURSOR_LEFT, false);
  EXPECT_EQ(SelectionModel(3, CURSOR_FORWARD), render_text->selection_model());

  // Moving the cursor by word from "|aCB" to the right should return "aCB|".
  // But since end of text is always treated as a word break, it returns
  // position "a|CB".
  // TODO(xji): Need to make it work as expected.
  render_text->MoveCursor(LINE_BREAK, CURSOR_LEFT, false);
  render_text->MoveCursor(WORD_BREAK, CURSOR_RIGHT, false);
  // EXPECT_EQ(SelectionModel(), render_text->selection_model());
}

TEST_F(RenderTextTest, MoveLeftRightByWordInTextWithMultiSpaces) {
  scoped_ptr<RenderText> render_text(RenderText::CreateRenderText());
  render_text->SetText(WideToUTF16(L"abc     def"));
  render_text->MoveCursorTo(SelectionModel(5, CURSOR_FORWARD));
  render_text->MoveCursor(WORD_BREAK, CURSOR_RIGHT, false);
  EXPECT_EQ(11U, render_text->cursor_position());

  render_text->MoveCursorTo(SelectionModel(5, CURSOR_FORWARD));
  render_text->MoveCursor(WORD_BREAK, CURSOR_LEFT, false);
  EXPECT_EQ(0U, render_text->cursor_position());
}

TEST_F(RenderTextTest, MoveLeftRightByWordInChineseText) {
  scoped_ptr<RenderText> render_text(RenderText::CreateRenderText());
  render_text->SetText(WideToUTF16(L"\x6211\x4EEC\x53BB\x516C\x56ED\x73A9"));
  render_text->MoveCursor(LINE_BREAK, CURSOR_LEFT, false);
  EXPECT_EQ(0U, render_text->cursor_position());
  render_text->MoveCursor(WORD_BREAK, CURSOR_RIGHT, false);
  EXPECT_EQ(2U, render_text->cursor_position());
  render_text->MoveCursor(WORD_BREAK, CURSOR_RIGHT, false);
  EXPECT_EQ(3U, render_text->cursor_position());
  render_text->MoveCursor(WORD_BREAK, CURSOR_RIGHT, false);
  EXPECT_EQ(5U, render_text->cursor_position());
  render_text->MoveCursor(WORD_BREAK, CURSOR_RIGHT, false);
  EXPECT_EQ(6U, render_text->cursor_position());
  render_text->MoveCursor(WORD_BREAK, CURSOR_RIGHT, false);
  EXPECT_EQ(6U, render_text->cursor_position());
}
#endif

TEST_F(RenderTextTest, StringSizeSanity) {
  scoped_ptr<RenderText> render_text(RenderText::CreateRenderText());
  render_text->SetText(UTF8ToUTF16("Hello World"));
  const Size string_size = render_text->GetStringSize();
  EXPECT_GT(string_size.width(), 0);
  EXPECT_GT(string_size.height(), 0);
}

TEST_F(RenderTextTest, StringSizeEmptyString) {
  const Font font;
  scoped_ptr<RenderText> render_text(RenderText::CreateRenderText());
  render_text->SetFontList(FontList(font));

  render_text->SetText(string16());
  EXPECT_EQ(font.GetHeight(), render_text->GetStringSize().height());
  EXPECT_EQ(0, render_text->GetStringSize().width());

  render_text->SetText(UTF8ToUTF16(" "));
  EXPECT_EQ(font.GetHeight(), render_text->GetStringSize().height());
}

TEST_F(RenderTextTest, StringSizeBoldWidth) {
  scoped_ptr<RenderText> render_text(RenderText::CreateRenderText());
  render_text->SetText(UTF8ToUTF16("Hello World"));

  const int plain_width = render_text->GetStringSize().width();
  EXPECT_GT(plain_width, 0);

  // Apply a bold style and check that the new width is greater.
  StyleRange bold;
  bold.font_style |= Font::BOLD;
  render_text->set_default_style(bold);
  render_text->ApplyDefaultStyle();

  const int bold_width = render_text->GetStringSize().width();
  EXPECT_GT(bold_width, plain_width);

  // Now, apply a plain style over the first word only.
  StyleRange plain;
  plain.font_style = Font::NORMAL;
  plain.range = ui::Range(0, 5);
  render_text->ApplyStyleRange(plain);

  const int plain_bold_width = render_text->GetStringSize().width();
  EXPECT_GT(plain_bold_width, plain_width);
  EXPECT_LT(plain_bold_width, bold_width);
}

TEST_F(RenderTextTest, StringSizeHeight) {
  struct {
    string16 text;
  } cases[] = {
    { WideToUTF16(L"Hello World!") },  // English
    { WideToUTF16(L"\x6328\x62f6") },  // Japanese
    { WideToUTF16(L"\x0915\x093f") },  // Hindi
    { WideToUTF16(L"\x05e0\x05b8") },  // Hebrew
  };

  Font default_font;
  Font larger_font = default_font.DeriveFont(24, default_font.GetStyle());
  EXPECT_GT(larger_font.GetHeight(), default_font.GetHeight());

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); i++) {
    scoped_ptr<RenderText> render_text(RenderText::CreateRenderText());
    render_text->SetFontList(FontList(default_font));
    render_text->SetText(cases[i].text);

    const int height1 = render_text->GetStringSize().height();
    EXPECT_GT(height1, 0);

    // Check that setting the larger font increases the height.
    render_text->SetFontList(FontList(larger_font));
    const int height2 = render_text->GetStringSize().height();
    EXPECT_GT(height2, height1);
  }
}

TEST_F(RenderTextTest, CursorBoundsInReplacementMode) {
  scoped_ptr<RenderText> render_text(RenderText::CreateRenderText());
  render_text->SetText(ASCIIToUTF16("abcdefg"));
  render_text->SetDisplayRect(Rect(100, 17));
  SelectionModel sel_b(1, CURSOR_FORWARD);
  SelectionModel sel_c(2, CURSOR_FORWARD);
  Rect cursor_around_b = render_text->GetCursorBounds(sel_b, false);
  Rect cursor_before_b = render_text->GetCursorBounds(sel_b, true);
  Rect cursor_before_c = render_text->GetCursorBounds(sel_c, true);
  EXPECT_EQ(cursor_around_b.x(), cursor_before_b.x());
  EXPECT_EQ(cursor_around_b.right(), cursor_before_c.x());
}

TEST_F(RenderTextTest, OriginForDrawing) {
  scoped_ptr<RenderText> render_text(RenderText::CreateRenderText());
  render_text->SetText(ASCIIToUTF16("abcdefg"));
  render_text->SetFontList(FontList("Arial, 13px"));

  // Set display area's height equals to font height.
  const int font_height = render_text->GetStringSize().height();
  Rect display_rect(0, 0, 100, font_height);
  render_text->SetDisplayRect(display_rect);

  Point origin = render_text->GetOriginForDrawing();
  EXPECT_EQ(origin.x(), 0);
  EXPECT_EQ(origin.y(), 0);

  // Set display area's height greater than font height.
  display_rect = Rect(0, 0, 100, font_height + 2);
  render_text->SetDisplayRect(display_rect);

  origin = render_text->GetOriginForDrawing();
  EXPECT_EQ(origin.x(), 0);
  EXPECT_EQ(origin.y(), 1);
}

TEST_F(RenderTextTest, SameFontForParentheses) {
  struct {
    const char16 left_char;
    const char16 right_char;
  } punctuation_pairs[] = {
    { '(', ')' },
    { '{', '}' },
    { '<', '>' },
  };
  struct {
    string16 text;
  } cases[] = {
    // English(English)
    { WideToUTF16(L"Hello World(a)") },
    // English(English)English
    { WideToUTF16(L"Hello World(a)Hello World") },

    // Japanese(English)
    { WideToUTF16(L"\x6328\x62f6(a)") },
    // Japanese(English)Japanese
    { WideToUTF16(L"\x6328\x62f6(a)\x6328\x62f6") },
    // English(Japanese)English
    { WideToUTF16(L"Hello World(\x6328\x62f6)Hello World") },

    // Hindi(English)
    { WideToUTF16(L"\x0915\x093f(a)") },
    // Hindi(English)Hindi
    { WideToUTF16(L"\x0915\x093f(a)\x0915\x093f") },
    // English(Hindi)English
    { WideToUTF16(L"Hello World(\x0915\x093f)Hello World") },

    // Hebrew(English)
    { WideToUTF16(L"\x05e0\x05b8(a)") },
    // Hebrew(English)Hebrew
    { WideToUTF16(L"\x05e0\x05b8(a)\x05e0\x05b8") },
    // English(Hebrew)English
    { WideToUTF16(L"Hello World(\x05e0\x05b8)Hello World") },
  };

  scoped_ptr<RenderText> render_text(RenderText::CreateRenderText());
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    string16 text = cases[i].text;
    const size_t start_paren_char_index = text.find('(');
    ASSERT_NE(string16::npos, start_paren_char_index);
    const size_t end_paren_char_index = text.find(')');
    ASSERT_NE(string16::npos, end_paren_char_index);

    for (size_t j = 0; j < ARRAYSIZE_UNSAFE(punctuation_pairs); ++j) {
      text[start_paren_char_index] = punctuation_pairs[j].left_char;
      text[end_paren_char_index] = punctuation_pairs[j].right_char;
      render_text->SetText(text);

      const std::vector<RenderText::FontSpan> spans =
          render_text->GetFontSpansForTesting();

      int start_paren_span_index = -1;
      int end_paren_span_index = -1;
      for (size_t k = 0; k < spans.size(); ++k) {
        if (IndexInRange(spans[k].second, start_paren_char_index))
          start_paren_span_index = k;
        if (IndexInRange(spans[k].second, end_paren_char_index))
          end_paren_span_index = k;
      }
      ASSERT_NE(-1, start_paren_span_index);
      ASSERT_NE(-1, end_paren_span_index);

      const Font& start_font = spans[start_paren_span_index].first;
      const Font& end_font = spans[end_paren_span_index].first;
      EXPECT_EQ(start_font.GetFontName(), end_font.GetFontName());
      EXPECT_EQ(start_font.GetFontSize(), end_font.GetFontSize());
      EXPECT_EQ(start_font.GetStyle(), end_font.GetStyle());
    }
  }
}

TEST_F(RenderTextTest, DisplayRectShowsCursorLTR) {
  scoped_ptr<RenderText> render_text(RenderText::CreateRenderText());
  render_text->SetText(WideToUTF16(L"abcdefghijklmnopqrstuvwxzyabcdefg"));
  render_text->MoveCursorTo(SelectionModel(render_text->text().length(),
                                           CURSOR_FORWARD));
  int width = render_text->GetStringSize().width();
  ASSERT_GT(width, 10);

  // Ensure that the cursor is placed at the width of its preceding text.
  render_text->SetDisplayRect(Rect(width + 10, 1));
  EXPECT_EQ(width, render_text->GetUpdatedCursorBounds().x());

  // Ensure that shrinking the display rectangle keeps the cursor in view.
  render_text->SetDisplayRect(Rect(width - 10, 1));
  EXPECT_EQ(render_text->display_rect().width() - 1,
            render_text->GetUpdatedCursorBounds().x());

  // Ensure that the text will pan to fill its expanding display rectangle.
  render_text->SetDisplayRect(Rect(width - 5, 1));
  EXPECT_EQ(render_text->display_rect().width() - 1,
            render_text->GetUpdatedCursorBounds().x());

  // Ensure that a sufficiently large display rectangle shows all the text.
  render_text->SetDisplayRect(Rect(width + 10, 1));
  EXPECT_EQ(width, render_text->GetUpdatedCursorBounds().x());

  // Repeat the test with RTL text.
  render_text->SetText(WideToUTF16(L"\x5d0\x5d1\x5d2\x5d3\x5d4\x5d5\x5d6\x5d7"
      L"\x5d8\x5d9\x5da\x5db\x5dc\x5dd\x5de\x5df"));
  render_text->MoveCursorTo(SelectionModel(0, CURSOR_FORWARD));
  width = render_text->GetStringSize().width();
  ASSERT_GT(width, 10);

  // Ensure that the cursor is placed at the width of its preceding text.
  render_text->SetDisplayRect(Rect(width + 10, 1));
  EXPECT_EQ(width, render_text->GetUpdatedCursorBounds().x());

  // Ensure that shrinking the display rectangle keeps the cursor in view.
  render_text->SetDisplayRect(Rect(width - 10, 1));
  EXPECT_EQ(render_text->display_rect().width() - 1,
            render_text->GetUpdatedCursorBounds().x());

  // Ensure that the text will pan to fill its expanding display rectangle.
  render_text->SetDisplayRect(Rect(width - 5, 1));
  EXPECT_EQ(render_text->display_rect().width() - 1,
            render_text->GetUpdatedCursorBounds().x());

  // Ensure that a sufficiently large display rectangle shows all the text.
  render_text->SetDisplayRect(Rect(width + 10, 1));
  EXPECT_EQ(width, render_text->GetUpdatedCursorBounds().x());
}

TEST_F(RenderTextTest, DisplayRectShowsCursorRTL) {
  // Set the locale to Hebrew for RTL UI.
  std::string locale = l10n_util::GetApplicationLocale("");
  base::i18n::SetICUDefaultLocale("he");

  scoped_ptr<RenderText> render_text(RenderText::CreateRenderText());
  render_text->SetText(WideToUTF16(L"abcdefghijklmnopqrstuvwxzyabcdefg"));
  render_text->MoveCursorTo(SelectionModel(0, CURSOR_FORWARD));
  int width = render_text->GetStringSize().width();
  ASSERT_GT(width, 10);

  // Ensure that the cursor is placed at the width of its preceding text.
  render_text->SetDisplayRect(Rect(width + 10, 1));
  EXPECT_EQ(render_text->display_rect().width() - width - 1,
            render_text->GetUpdatedCursorBounds().x());

  // Ensure that shrinking the display rectangle keeps the cursor in view.
  render_text->SetDisplayRect(Rect(width - 10, 1));
  EXPECT_EQ(0, render_text->GetUpdatedCursorBounds().x());

  // Ensure that the text will pan to fill its expanding display rectangle.
  render_text->SetDisplayRect(Rect(width - 5, 1));
  EXPECT_EQ(0, render_text->GetUpdatedCursorBounds().x());

  // Ensure that a sufficiently large display rectangle shows all the text.
  render_text->SetDisplayRect(Rect(width + 10, 1));
  EXPECT_EQ(render_text->display_rect().width() - width - 1,
            render_text->GetUpdatedCursorBounds().x());

  // Repeat the test with RTL text.
  render_text->SetText(WideToUTF16(L"\x5d0\x5d1\x5d2\x5d3\x5d4\x5d5\x5d6\x5d7"
      L"\x5d8\x5d9\x5da\x5db\x5dc\x5dd\x5de\x5df"));
  render_text->MoveCursorTo(SelectionModel(render_text->text().length(),
                                           CURSOR_FORWARD));
  width = render_text->GetStringSize().width();
  ASSERT_GT(width, 10);

  // Ensure that the cursor is placed at the width of its preceding text.
  render_text->SetDisplayRect(Rect(width + 10, 1));
  EXPECT_EQ(render_text->display_rect().width() - width - 1,
            render_text->GetUpdatedCursorBounds().x());

  // Ensure that shrinking the display rectangle keeps the cursor in view.
  render_text->SetDisplayRect(Rect(width - 10, 1));
  EXPECT_EQ(0, render_text->GetUpdatedCursorBounds().x());

  // Ensure that the text will pan to fill its expanding display rectangle.
  render_text->SetDisplayRect(Rect(width - 5, 1));
  EXPECT_EQ(0, render_text->GetUpdatedCursorBounds().x());

  // Ensure that a sufficiently large display rectangle shows all the text.
  render_text->SetDisplayRect(Rect(width + 10, 1));
  EXPECT_EQ(render_text->display_rect().width() - width - 1,
            render_text->GetUpdatedCursorBounds().x());

  // Reset locale.
  base::i18n::SetICUDefaultLocale(locale);
}

}  // namespace gfx
