// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Cocoa/Cocoa.h>

#include "ui/gfx/font.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(PlatformFontMacTest, DeriveFont) {
  // Use a base font that support all traits.
  gfx::Font base_font("Helvetica", 13);

  // Bold
  gfx::Font bold_font(base_font.DeriveFont(0, gfx::Font::BOLD));
  NSFontTraitMask traits = [[NSFontManager sharedFontManager]
      traitsOfFont:bold_font.GetNativeFont()];
  EXPECT_EQ(NSBoldFontMask, traits);

  // Italic
  gfx::Font italic_font(base_font.DeriveFont(0, gfx::Font::ITALIC));
  traits = [[NSFontManager sharedFontManager]
      traitsOfFont:italic_font.GetNativeFont()];
  EXPECT_EQ(NSItalicFontMask, traits);

  // Bold italic
  gfx::Font bold_italic_font(base_font.DeriveFont(0, gfx::Font::BOLD |
                                                     gfx::Font::ITALIC));
  traits = [[NSFontManager sharedFontManager]
      traitsOfFont:bold_italic_font.GetNativeFont()];
  EXPECT_EQ(static_cast<NSFontTraitMask>(NSBoldFontMask | NSItalicFontMask),
            traits);
}
