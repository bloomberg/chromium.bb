// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_FONT_FALLBACK_H_
#define CHROME_BROWSER_VR_FONT_FALLBACK_H_

#include <set>
#include <string>
#include "base/strings/string16.h"
#include "third_party/icu/source/common/unicode/uchar.h"

namespace gfx {
class FontList;
}  // namespace gfx

namespace vr {

// Just convert a string to a set of unique characters, used for each platform-
// specific implementation of GetFontList.
std::set<UChar32> CollectDifferentChars(base::string16 text);

// Attempt to find a font list that supports all characters in the text. If
// validate_fonts_contain_all_code_points == true, actually iterates all
// characters to ensure they have valid glyphs in the fonts.
bool GetFontList(const std::string& preferred_font_name,
                 int font_size,
                 base::string16 text,
                 gfx::FontList* font_list,
                 bool validate_fonts_contain_all_code_points);

}  // namespace vr

#endif  // CHROME_BROWSER_VR_FONT_FALLBACK_H_
