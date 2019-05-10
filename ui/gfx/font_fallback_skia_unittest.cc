// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_fallback.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gfx {

namespace {

static const wchar_t* kFallbackFontTests[] = {
    L"\u0540\u0541",  // Armenian,
    L"\u0631\u0632",  // Arabic
    L"\u0915\u093f",  // Devanagari
    L"\u5203\u5204",  // CJK Unified Ideograph
};

}  // namespace

TEST(FontFallbackSkiaTest, EmptyStringFallback) {
  Font base_font;
  Font fallback_font;
  bool result =
      GetFallbackFont(base_font, base::StringPiece16(), &fallback_font);
  EXPECT_FALSE(result);
}

TEST(FontFallbackSkiaTest, FontFallback) {
  for (const auto* test : kFallbackFontTests) {
    Font base_font;
    Font fallback_font;
    base::string16 text = base::WideToUTF16(test);

    if (!GetFallbackFont(base_font, text, &fallback_font)) {
      ADD_FAILURE() << "Font fallback failed: '" << text << "'";
    }
  }
}

}  // namespace gfx
