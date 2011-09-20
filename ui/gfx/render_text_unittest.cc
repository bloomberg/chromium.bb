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
  scoped_ptr<gfx::RenderText> render_text(gfx::RenderText::CreateRenderText());
  EXPECT_TRUE(render_text->text().empty());
  EXPECT_TRUE(render_text->style_ranges().empty());

  // Test that the built-in default style is applied for new text.
  render_text->SetText(ASCIIToUTF16("abc"));
  EXPECT_EQ(1U, render_text->style_ranges().size());
  gfx::StyleRange style;
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
  scoped_ptr<gfx::RenderText> render_text(gfx::RenderText::CreateRenderText());
  gfx::StyleRange color;
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
  gfx::StyleRange strike;
  strike.strike = true;
  render_text->set_default_style(strike);
  render_text->ApplyDefaultStyle();
  EXPECT_EQ(1U, render_text->style_ranges().size());
  EXPECT_EQ(true, render_text->style_ranges()[0].strike);
  EXPECT_EQ(strike.foreground, render_text->style_ranges()[0].foreground);
}

TEST_F(RenderTextTest, ApplyStyleRange) {
  scoped_ptr<gfx::RenderText> render_text(gfx::RenderText::CreateRenderText());
  render_text->SetText(ASCIIToUTF16("01234"));
  EXPECT_EQ(1U, render_text->style_ranges().size());

  // Test ApplyStyleRange (no-op on empty range).
  gfx::StyleRange empty;
  empty.range = ui::Range(1, 1);
  render_text->ApplyStyleRange(empty);
  EXPECT_EQ(1U, render_text->style_ranges().size());

  // Test ApplyStyleRange (no-op on invalid range).
  gfx::StyleRange invalid;
  invalid.range = ui::Range::InvalidRange();
  render_text->ApplyStyleRange(invalid);
  EXPECT_EQ(1U, render_text->style_ranges().size());

  // Apply a style with a range contained by an existing range.
  gfx::StyleRange underline;
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
  gfx::StyleRange color;
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
  gfx::StyleRange strike;
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
  gfx::StyleRange strike_underline;
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
  scoped_ptr<gfx::RenderText> render_text(gfx::RenderText::CreateRenderText());
  render_text->SetText(ASCIIToUTF16("abcdef"));
  EXPECT_EQ(1U, render_text->style_ranges().size());
  EXPECT_EQ(ui::Range(0, 6), render_text->style_ranges()[0].range);

  // Test that the range is clipped to the length of shorter text.
  render_text->SetText(ASCIIToUTF16("abc"));
  EXPECT_EQ(1U, render_text->style_ranges().size());
  EXPECT_EQ(ui::Range(0, 3), render_text->style_ranges()[0].range);

  // Test that the last range extends to the length of longer text.
  gfx::StyleRange strike;
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

}  // namespace gfx
