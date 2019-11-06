// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_fallback.h"

#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/icu/source/common/unicode/uscript.h"
#include "third_party/icu/source/common/unicode/utf16.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "ui/gfx/font.h"

namespace gfx {

std::vector<Font> GetFallbackFonts(const Font& font) {
  return std::vector<Font>();
}

bool GetFallbackFont(const Font& font,
                     const std::string& locale,
                     base::StringPiece16 text,
                     Font* result) {
  TRACE_EVENT0("fonts", "gfx::GetFallbackFont");

  if (text.empty())
    return false;

  sk_sp<SkFontMgr> font_mgr(SkFontMgr::RefDefault());

  const char* bcp47_locales[] = {locale.c_str()};
  int num_locales = locale.empty() ? 0 : 1;
  const char** locales = locale.empty() ? nullptr : bcp47_locales;

  const int font_weight = (font.GetWeight() == Font::Weight::INVALID)
                              ? static_cast<int>(Font::Weight::NORMAL)
                              : static_cast<int>(font.GetWeight());
  const bool italic = (font.GetStyle() & Font::ITALIC) != 0;
  SkFontStyle skia_style(
      font_weight, SkFontStyle::kNormal_Width,
      italic ? SkFontStyle::kItalic_Slant : SkFontStyle::kUpright_Slant);

  std::set<SkFontID> tested_typeface;
  SkString skia_family_name;
  size_t fewest_missing_glyphs = text.length() + 1;

  size_t offset = 0;
  while (offset < text.length()) {
    UChar32 code_point;
    U16_NEXT(text.data(), offset, text.length(), code_point);

    sk_sp<SkTypeface> typeface(font_mgr->matchFamilyStyleCharacter(
        font.GetFontName().c_str(), skia_style, locales, num_locales,
        code_point));
    // If the typeface is not found or was already tested, skip it.
    if (!typeface || !tested_typeface.insert(typeface->uniqueID()).second)
      continue;

    // Validate that every character has a known glyph in the font.
    size_t missing_glyphs = 0;
    size_t i = 0;
    while (i < text.length()) {
      UChar32 c;
      U16_NEXT(text.data(), i, text.length(), c);
      if (typeface->unicharToGlyph(c) == 0)
        ++missing_glyphs;
    }

    if (missing_glyphs < fewest_missing_glyphs) {
      fewest_missing_glyphs = missing_glyphs;
      typeface->getFamilyName(&skia_family_name);
    }

    // The font is a valid fallback font for the given text.
    if (missing_glyphs == 0)
      break;
  }

  if (!skia_family_name.isEmpty()) {
    *result =
        Font(std::string(skia_family_name.c_str(), skia_family_name.size()),
             font.GetFontSize());
    return true;
  }

  return false;
}

}  // namespace gfx
