// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/font_fallback.h"

#include <string>
#include <vector>

#include "chrome/browser/vr/ui_support.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/icu/source/common/unicode/uscript.h"
#include "third_party/icu/source/common/unicode/utf16.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_fallback.h"
#include "ui/gfx/font_fallback_linux.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/platform_font_skia.h"

namespace vr {

bool GetFontList(const std::string& preferred_font_name,
                 int font_size,
                 base::string16 text,
                 gfx::FontList* font_list,
                 bool validate_fonts_contain_all_code_points) {
  gfx::Font preferred_font(preferred_font_name, font_size);
  if (validate_fonts_contain_all_code_points) {
    // TODO(billorr): Consider caching results like the Android path.
    for (const UChar32 c : CollectDifferentChars(text)) {
      // Validate that this is a valid unicode character.
      UErrorCode err = UErrorCode::U_ZERO_ERROR;
      UScriptCode script = UScriptGetScript(c, &err);
      if (!U_SUCCESS(err) || script == UScriptCode::USCRIPT_INVALID_CODE)
        return false;

      gfx::FallbackFontData data = gfx::GetFallbackFontForChar(c, "");
      if (data.name.empty())
        return false;
    }
  }

  // Linux implements gfx::GetFallbackFonts with a non-trivial implementation,
  // and that is used to find fallbacks at render time.
  std::vector<gfx::Font> fonts{preferred_font};
  *font_list = gfx::FontList(fonts);
  return true;
}

}  // namespace vr
