// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_fallback_linux.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_fallback.h"

namespace gfx {

namespace {
const char kDefaultApplicationLocale[] = "us-en";
}  // namespace

// If the Type 1 Symbol.pfb font is installed, it is returned as fallback font
// for the PUA character 0xf6db. This test ensures we're not returning Type 1
// fonts as fallback.
TEST(FontFallbackLinuxTest, NoType1InFallbackFonts) {
  FallbackFontData font_fallback_data =
      GetFallbackFontForChar(0xf6db, std::string());
  if (font_fallback_data.filename.length() >= 3u) {
    std::string extension = font_fallback_data.filename.substr(
        font_fallback_data.filename.length() - 3);
    EXPECT_NE(extension, "pfb");
  } else {
    EXPECT_EQ(font_fallback_data.filename.length(), 0u);
  }
}

TEST(FontFallbackLinuxTest, GetFallbackFont) {
  Font base_font;

  Font fallback_font_cjk;
  EXPECT_TRUE(GetFallbackFont(base_font, kDefaultApplicationLocale,
                              base::WideToUTF16(L"⻩"), &fallback_font_cjk));
  EXPECT_EQ(fallback_font_cjk.GetFontName(), "Noto Sans CJK JP");

  Font fallback_font_khmer;
  EXPECT_TRUE(GetFallbackFont(base_font, kDefaultApplicationLocale,
                              base::WideToUTF16(L"ឨឮឡ"), &fallback_font_khmer));
  EXPECT_EQ(fallback_font_khmer.GetFontName(), "Noto Sans Khmer");
}

TEST(FontFallbackLinuxTest, Fallbacks) {
  Font default_font("sans", 13);
  std::vector<Font> fallbacks = GetFallbackFonts(default_font);
  EXPECT_FALSE(fallbacks.empty());

  // The first fallback should be 'DejaVu Sans' which is the default linux
  // fonts. The fonts on linux are mock with test_fonts (see
  // third_party/tests_font).
  if (!fallbacks.empty())
    EXPECT_EQ(fallbacks[0].GetFontName(), "DejaVu Sans");
}

}  // namespace gfx
