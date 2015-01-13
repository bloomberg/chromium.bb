// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/platform_font_win.h"

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_hdc.h"
#include "base/win/scoped_select_object.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/font.h"
#include "ui/gfx/win/direct_write.h"
#include "ui/gfx/win/scoped_set_map_mode.h"

namespace gfx {

namespace {

// Returns a font based on |base_font| with height at most |target_height| and
// font size maximized. Returns |base_font| if height is already equal.
gfx::Font AdjustFontSizeForHeight(const gfx::Font& base_font,
                                  int target_height) {
  Font expected_font = base_font;
  if (base_font.GetHeight() < target_height) {
    // Increase size while height is <= |target_height|.
    Font larger_font = base_font.Derive(1, 0);
    while (larger_font.GetHeight() <= target_height) {
      expected_font = larger_font;
      larger_font = larger_font.Derive(1, 0);
    }
  } else if (expected_font.GetHeight() > target_height) {
    // Decrease size until height is <= |target_height|.
    do {
      expected_font = expected_font.Derive(-1, 0);
    } while (expected_font.GetHeight() > target_height);
  }
  return expected_font;
}

}  // namespace

TEST(PlatformFontWinTest, DeriveFontWithHeight) {
  // TODO(ananta): Fix this test for DirectWrite. http://crbug.com/442010
  if (gfx::win::IsDirectWriteEnabled())
    return;

  const Font base_font;
  PlatformFontWin* platform_font =
      static_cast<PlatformFontWin*>(base_font.platform_font());

  for (int i = -10; i < 10; i++) {
    const int target_height = base_font.GetHeight() + i;
    Font expected_font = AdjustFontSizeForHeight(base_font, target_height);
    ASSERT_LE(expected_font.GetHeight(), target_height);

    Font derived_font = platform_font->DeriveFontWithHeight(target_height, 0);
    EXPECT_EQ(expected_font.GetFontName(), derived_font.GetFontName());
    EXPECT_EQ(expected_font.GetFontSize(), derived_font.GetFontSize());
    EXPECT_LE(expected_font.GetHeight(), target_height);
    EXPECT_EQ(0, derived_font.GetStyle());

    derived_font = platform_font->DeriveFontWithHeight(target_height,
                                                       Font::BOLD);
    EXPECT_EQ(expected_font.GetFontName(), derived_font.GetFontName());
    EXPECT_EQ(expected_font.GetFontSize(), derived_font.GetFontSize());
    EXPECT_LE(expected_font.GetHeight(), target_height);
    EXPECT_EQ(Font::BOLD, derived_font.GetStyle());

    // Test that deriving from the new font has the expected result.
    Font rederived_font = derived_font.Derive(1, 0);
    expected_font = Font(derived_font.GetFontName(),
                         derived_font.GetFontSize() + 1);
    EXPECT_EQ(expected_font.GetFontName(), rederived_font.GetFontName());
    EXPECT_EQ(expected_font.GetFontSize(), rederived_font.GetFontSize());
    EXPECT_EQ(expected_font.GetHeight(), rederived_font.GetHeight());
  }
}

TEST(PlatformFontWinTest, DeriveFontWithHeight_Consistency) {
  // TODO(ananta): Fix this test for DirectWrite. http://crbug.com/442010
  if (gfx::win::IsDirectWriteEnabled())
    return;
  gfx::Font arial_12("Arial", 12);
  ASSERT_GT(16, arial_12.GetHeight());
  gfx::Font derived_1 = static_cast<PlatformFontWin*>(
      arial_12.platform_font())->DeriveFontWithHeight(16, 0);

  gfx::Font arial_15("Arial", 15);
  ASSERT_LT(16, arial_15.GetHeight());
  gfx::Font derived_2 = static_cast<PlatformFontWin*>(
      arial_15.platform_font())->DeriveFontWithHeight(16, 0);

  EXPECT_EQ(derived_1.GetFontSize(), derived_2.GetFontSize());
  EXPECT_EQ(16, derived_1.GetHeight());
  EXPECT_EQ(16, derived_2.GetHeight());
}

// Callback function used by DeriveFontWithHeight_MinSize() below.
static int GetMinFontSize() {
  return 10;
}

TEST(PlatformFontWinTest, DeriveFontWithHeight_MinSize) {
  PlatformFontWin::GetMinimumFontSizeCallback old_callback =
    PlatformFontWin::get_minimum_font_size_callback;
  PlatformFontWin::get_minimum_font_size_callback = &GetMinFontSize;

  const Font base_font;
  const Font min_font(base_font.GetFontName(), GetMinFontSize());
  PlatformFontWin* platform_font =
      static_cast<PlatformFontWin*>(base_font.platform_font());

  const Font derived_font =
      platform_font->DeriveFontWithHeight(min_font.GetHeight() - 1, 0);
  EXPECT_EQ(min_font.GetFontSize(), derived_font.GetFontSize());
  EXPECT_EQ(min_font.GetHeight(), derived_font.GetHeight());

  PlatformFontWin::get_minimum_font_size_callback = old_callback;
}

TEST(PlatformFontWinTest, DeriveFontWithHeight_TooSmall) {
  const Font base_font;
  PlatformFontWin* platform_font =
      static_cast<PlatformFontWin*>(base_font.platform_font());

  const Font derived_font = platform_font->DeriveFontWithHeight(1, 0);
  EXPECT_GT(derived_font.GetHeight(), 1);
}

// Test whether font metrics retrieved by DirectWrite (skia) and GDI match as
// per assumptions mentioned below:-
// 1. Font size is the same
// 2. The difference between GDI and DirectWrite for font height, baseline,
//    and cap height is at most 1. For smaller font sizes under 12, GDI
//    font heights/baselines/cap height are equal/larger by 1 point. For larger
//    font sizes DirectWrite font heights/baselines/cap height are equal/larger
//    by 1 point.
TEST(PlatformFontWinTest, Metrics_SkiaVersusGDI) {
  if (!gfx::win::IsDirectWriteEnabled())
    return;

  // Describes the font being tested.
  struct FontInfo {
    base::string16 font_name;
    int font_size;
  };

  FontInfo fonts[] = {
    {base::ASCIIToUTF16("Arial"), 6},
    {base::ASCIIToUTF16("Arial"), 8},
    {base::ASCIIToUTF16("Arial"), 10},
    {base::ASCIIToUTF16("Arial"), 12},
    {base::ASCIIToUTF16("Arial"), 16},
    {base::ASCIIToUTF16("Symbol"), 6},
    {base::ASCIIToUTF16("Symbol"), 10},
    {base::ASCIIToUTF16("Symbol"), 12},
    {base::ASCIIToUTF16("Tahoma"), 10},
    {base::ASCIIToUTF16("Tahoma"), 16},
    {base::ASCIIToUTF16("Segoe UI"), 6},
    {base::ASCIIToUTF16("Segoe UI"), 8},
    {base::ASCIIToUTF16("Segoe UI"), 20},
  };

  base::win::ScopedGetDC screen_dc(NULL);
  gfx::ScopedSetMapMode mode(screen_dc, MM_TEXT);

  for (int i = 0; i < arraysize(fonts); ++i) {
    LOGFONT font_info = {0};

    font_info.lfHeight = -fonts[i].font_size;
    font_info.lfWeight = FW_NORMAL;
    wcscpy_s(font_info.lfFaceName,
             fonts[i].font_name.length() + 1,
             fonts[i].font_name.c_str());

    HFONT font = CreateFontIndirect(&font_info);

    TEXTMETRIC font_metrics;
    PlatformFontWin::GetTextMetricsForFont(screen_dc, font, &font_metrics);

    scoped_refptr<PlatformFontWin::HFontRef> h_font_gdi(
        PlatformFontWin::CreateHFontRefFromGDI(font, font_metrics));

    scoped_refptr<PlatformFontWin::HFontRef> h_font_skia(
        PlatformFontWin::CreateHFontRefFromSkia(font, font_metrics));

    EXPECT_EQ(h_font_gdi->font_size(), h_font_skia->font_size());
    EXPECT_EQ(h_font_gdi->style(), h_font_skia->style());
    EXPECT_EQ(h_font_gdi->font_name(), h_font_skia->font_name());

    EXPECT_LE(abs(h_font_gdi->cap_height() - h_font_skia->cap_height()), 1);
    EXPECT_LE(abs(h_font_gdi->baseline() - h_font_skia->baseline()), 1);
    EXPECT_LE(abs(h_font_gdi->height() - h_font_skia->height()), 1);
  }
}


}  // namespace gfx
