// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_list.h"

#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace gfx {

typedef testing::Test FontListTest;

TEST_F(FontListTest, FontDescString_FromDescString) {
  // Test init from font name style size string.
  FontList font_list = FontList("Droid Sans serif, Sans serif, 10px");
  const std::string& font_str = font_list.GetFontDescriptionString();
  EXPECT_EQ("Droid Sans serif, Sans serif, 10px", font_str);
}

TEST_F(FontListTest, FontDescString_FromFont) {
  // Test init from Font.
  Font font("Arial", 8);
  FontList font_list = FontList(font);
  const std::string& font_str = font_list.GetFontDescriptionString();
  EXPECT_EQ("Arial,8px", font_str);
}

TEST_F(FontListTest, FontDescString_FromFontWithNonNormalStyle) {
  // Test init from Font with non-normal style.
  Font font("Arial", 8);
  FontList font_list = FontList(font.DeriveFont(2, Font::BOLD));
  const std::string& font_str = font_list.GetFontDescriptionString();
#if defined(OS_LINUX)
  EXPECT_EQ("Arial,PANGO_WEIGHT_BOLD 10px", font_str);
#else
  EXPECT_EQ("Arial,10px", font_str);
#endif

  font_list = FontList(font.DeriveFont(-2, Font::ITALIC));
  const std::string& font_str_1 = font_list.GetFontDescriptionString();
#if defined(OS_LINUX)
  EXPECT_EQ("Arial,PANGO_STYLE_ITALIC 6px", font_str_1);
#else
  EXPECT_EQ("Arial,6px", font_str_1);
#endif
}

TEST_F(FontListTest, FontDescString_FromFontVector) {
  // Test init from Font vector.
  Font font("Arial", 8);
  Font font_1("Sans serif", 10);
  std::vector<Font> fonts;
  fonts.push_back(font.DeriveFont(0, Font::BOLD));
  fonts.push_back(font_1.DeriveFont(-2, Font::BOLD));
  FontList font_list = FontList(fonts);
  const std::string& font_str = font_list.GetFontDescriptionString();
#if defined(OS_LINUX)
  EXPECT_EQ("Arial,Sans serif,PANGO_WEIGHT_BOLD 8px", font_str);
#else
  EXPECT_EQ("Arial,Sans serif,8px", font_str);
#endif
}

TEST_F(FontListTest, Fonts_FromDescString) {
  // Test init from font name size string.
  FontList font_list = FontList("serif,Sans serif, 13px");
  const std::vector<Font>& fonts = font_list.GetFonts();
  EXPECT_EQ(2U, fonts.size());
  EXPECT_EQ("serif", fonts[0].GetFontName());
  EXPECT_EQ(13, fonts[0].GetFontSize());
  EXPECT_EQ(Font::NORMAL, fonts[0].GetStyle());
  EXPECT_EQ("Sans serif", fonts[1].GetFontName());
  EXPECT_EQ(13, fonts[1].GetFontSize());
  EXPECT_EQ(Font::NORMAL, fonts[1].GetStyle());
}

TEST_F(FontListTest, Fonts_FromDescStringInFlexibleFormat) {
  // Test init from font name size string with flexible format.
  FontList font_list = FontList("  serif   ,   Sans serif ,   13px");
  const std::vector<Font>& fonts = font_list.GetFonts();
  EXPECT_EQ(2U, fonts.size());
  EXPECT_EQ("serif", fonts[0].GetFontName());
  EXPECT_EQ(13, fonts[0].GetFontSize());
  EXPECT_EQ(Font::NORMAL, fonts[0].GetStyle());
  EXPECT_EQ("Sans serif", fonts[1].GetFontName());
  EXPECT_EQ(13, fonts[1].GetFontSize());
  EXPECT_EQ(Font::NORMAL, fonts[1].GetStyle());
}

TEST_F(FontListTest, Fonts_FromDescStringWithStyleInFlexibleFormat) {
  // Test init from font name style size string with flexible format.
  FontList font_list = FontList("  serif  ,  Sans serif ,  PANGO_WEIGHT_BOLD   "
                                "  PANGO_STYLE_ITALIC   13px");
  const std::vector<Font>& fonts = font_list.GetFonts();
  EXPECT_EQ(2U, fonts.size());
  EXPECT_EQ("serif", fonts[0].GetFontName());
  EXPECT_EQ(13, fonts[0].GetFontSize());
#if defined(OS_LINUX)
  EXPECT_EQ(Font::BOLD | Font::ITALIC, fonts[0].GetStyle());
#else
  EXPECT_EQ(Font::NORMAL, fonts[0].GetStyle());
#endif
  EXPECT_EQ("Sans serif", fonts[1].GetFontName());
  EXPECT_EQ(13, fonts[1].GetFontSize());
#if defined(OS_LINUX)
  EXPECT_EQ(Font::BOLD | Font::ITALIC, fonts[1].GetStyle());
#else
  EXPECT_EQ(Font::NORMAL, fonts[1].GetStyle());
#endif
}

TEST_F(FontListTest, Fonts_FromFont) {
  // Test init from Font.
  Font font("Arial", 8);
  FontList font_list = FontList(font);
  const std::vector<Font>& fonts = font_list.GetFonts();
  EXPECT_EQ(1U, fonts.size());
  EXPECT_EQ("Arial", fonts[0].GetFontName());
  EXPECT_EQ(8, fonts[0].GetFontSize());
  EXPECT_EQ(Font::NORMAL, fonts[0].GetStyle());
}

TEST_F(FontListTest, Fonts_FromFontWithNonNormalStyle) {
  // Test init from Font with non-normal style.
  Font font("Arial", 8);
  FontList font_list = FontList(font.DeriveFont(2, Font::BOLD));
  const std::vector<Font>& fonts = font_list.GetFonts();
  EXPECT_EQ(1U, fonts.size());
  EXPECT_EQ("Arial", fonts[0].GetFontName());
  EXPECT_EQ(10, fonts[0].GetFontSize());
  EXPECT_EQ(Font::BOLD, fonts[0].GetStyle());

  font_list = FontList(font.DeriveFont(-2, Font::ITALIC));
  const std::vector<Font>& fonts_1 = font_list.GetFonts();
  EXPECT_EQ(1U, fonts_1.size());
  EXPECT_EQ("Arial", fonts_1[0].GetFontName());
  EXPECT_EQ(6, fonts_1[0].GetFontSize());
  EXPECT_EQ(Font::ITALIC, fonts_1[0].GetStyle());
}

TEST_F(FontListTest, Fonts_FromFontVector) {
  // Test init from Font vector.
  Font font("Arial", 8);
  Font font_1("Sans serif", 10);
  std::vector<Font> input_fonts;
  input_fonts.push_back(font.DeriveFont(0, Font::BOLD));
  input_fonts.push_back(font_1.DeriveFont(-2, Font::BOLD));
  FontList font_list = FontList(input_fonts);
  const std::vector<Font>& fonts = font_list.GetFonts();
  EXPECT_EQ(2U, fonts.size());
  EXPECT_EQ("Arial", fonts[0].GetFontName());
  EXPECT_EQ(8, fonts[0].GetFontSize());
  EXPECT_EQ(Font::BOLD, fonts[0].GetStyle());
  EXPECT_EQ("Sans serif", fonts[1].GetFontName());
  EXPECT_EQ(8, fonts[1].GetFontSize());
  EXPECT_EQ(Font::BOLD, fonts[1].GetStyle());
}

TEST_F(FontListTest, Fonts_DescStringWithStyleInFlexibleFormat_RoundTrip) {
  // Test round trip from font description string to font vector to
  // font description string.
  FontList font_list = FontList("  serif  ,  Sans serif ,  PANGO_WEIGHT_BOLD   "
                                "  PANGO_STYLE_ITALIC   13px");

  const std::vector<Font>& fonts = font_list.GetFonts();
  FontList font_list_1 = FontList(fonts);
  const std::string& desc_str = font_list_1.GetFontDescriptionString();

#if defined(OS_LINUX)
  EXPECT_EQ("serif,Sans serif,PANGO_WEIGHT_BOLD PANGO_STYLE_ITALIC 13px",
            desc_str);
#else
  EXPECT_EQ("serif,Sans serif,13px", desc_str);
#endif
}

TEST_F(FontListTest, Fonts_FontVector_RoundTrip) {
  // Test round trip from font vector to font description string to font vector.
  Font font("Arial", 8);
  Font font_1("Sans serif", 10);
  std::vector<Font> input_fonts;
  input_fonts.push_back(font.DeriveFont(0, Font::BOLD));
  input_fonts.push_back(font_1.DeriveFont(-2, Font::BOLD));
  FontList font_list = FontList(input_fonts);

  const std::string& desc_string = font_list.GetFontDescriptionString();
  FontList font_list_1 = FontList(desc_string);
  const std::vector<Font>& round_trip_fonts = font_list_1.GetFonts();

  EXPECT_EQ(2U, round_trip_fonts.size());
  EXPECT_EQ("Arial", round_trip_fonts[0].GetFontName());
  EXPECT_EQ(8, round_trip_fonts[0].GetFontSize());
  EXPECT_EQ("Sans serif", round_trip_fonts[1].GetFontName());
  EXPECT_EQ(8, round_trip_fonts[1].GetFontSize());
#if defined(OS_LINUX)
  EXPECT_EQ(Font::BOLD, round_trip_fonts[0].GetStyle());
  EXPECT_EQ(Font::BOLD, round_trip_fonts[1].GetStyle());
#else
  // Style is ignored.
  EXPECT_EQ(Font::NORMAL, round_trip_fonts[0].GetStyle());
  EXPECT_EQ(Font::NORMAL, round_trip_fonts[1].GetStyle());
#endif
}

}  // namespace gfx
