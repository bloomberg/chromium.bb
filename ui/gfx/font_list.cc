// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "ui/gfx/font_list.h"

namespace gfx {

FontList::FontList() {
  fonts_.push_back(Font());
}

FontList::FontList(const std::string& font_description_string)
    : font_description_string_(font_description_string) {
  DCHECK(!font_description_string.empty());
  // DCHECK description string ends with "px" for size in pixel.
  DCHECK(EndsWith(font_description_string, "px", true));
}

FontList::FontList(const std::vector<Font>& fonts)
    : fonts_(fonts) {
  DCHECK(!fonts.empty());
  if (DCHECK_IS_ON()) {
    int style = fonts[0].GetStyle();
    int size = fonts[0].GetFontSize();
    for (size_t i = 1; i < fonts.size(); ++i) {
      DCHECK_EQ(fonts[i].GetStyle(), style);
      DCHECK_EQ(fonts[i].GetFontSize(), size);
    }
  }
}

FontList::FontList(const Font& font) {
  fonts_.push_back(font);
}

FontList::~FontList() {
}

const std::string& FontList::GetFontDescriptionString() const {
  if (font_description_string_.empty()) {
    DCHECK(!fonts_.empty());
    for (size_t i = 0; i < fonts_.size(); ++i) {
      std::string name = fonts_[i].GetFontName();
      font_description_string_ += name;
      font_description_string_ += ',';
    }
    // All fonts have the same style and size.
    // TODO(xji): add style for Windows.
#if defined(OS_LINUX)
    int style = fonts_[0].GetStyle();
    if (style & Font::BOLD)
      font_description_string_ += "PANGO_WEIGHT_BOLD ";
    if (style & Font::ITALIC)
      font_description_string_ += "PANGO_STYLE_ITALIC ";
#endif
    int size = fonts_[0].GetFontSize();
    font_description_string_ += base::IntToString(size);
    font_description_string_ += "px";
  }
  return font_description_string_;
}

const std::vector<Font>& FontList::GetFonts() const {
  if (fonts_.empty()) {
    DCHECK(!font_description_string_.empty());

    std::vector<std::string> name_style_size;
    base::SplitString(font_description_string_, ',', &name_style_size);
    int item_count = static_cast<int>(name_style_size.size());
    DCHECK_GT(item_count, 1);

    // The last item is [STYLE_OPTIONS] SIZE.
    std::vector<std::string> styles_size;
    base::SplitString(name_style_size[item_count - 1], ' ', &styles_size);
    DCHECK(!styles_size.empty());

    int style = 0;
    // TODO(xji): parse style for Windows.
#if defined(OS_LINUX)
    // Besides underline (which is supported through StyleRange), Font only
    // supports BOLD and ITALIC styles, not other Pango styles.
    for (size_t i = 0; i < styles_size.size() - 1; ++i) {
      // Styles are separated by white spaces. base::SplitString splits styles
      // by space, and it inserts empty string for continuous spaces.
      if (styles_size[i].empty())
        continue;
      if (!styles_size[i].compare("PANGO_WEIGHT_BOLD"))
        style |= Font::BOLD;
      else if (!styles_size[i].compare("PANGO_STYLE_ITALIC"))
        style |= Font::ITALIC;
      else
        NOTREACHED();
    }
#endif

    std::string font_size = styles_size[styles_size.size() - 1];
    int size_in_pixels;
    base::StringToInt(font_size, &size_in_pixels);
    DCHECK_GT(size_in_pixels, 0);

    for (int i = 0; i < item_count - 1; ++i) {
      DCHECK(!name_style_size[i].empty());

      Font font(name_style_size[i], size_in_pixels);
      if (style == Font::NORMAL)
        fonts_.push_back(font);
      else
        fonts_.push_back(font.DeriveFont(0, style));
    }
  }
  return fonts_;
}

}  // namespace gfx
