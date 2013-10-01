// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_list.h"

#include <algorithm>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace {

// Parses font description into |font_names|, |font_style| and |font_size|.
void ParseFontDescriptionString(const std::string& font_description_string,
                                std::vector<std::string>* font_names,
                                int* font_style,
                                int* font_size) {
  base::SplitString(font_description_string, ',', font_names);
  DCHECK_GT(font_names->size(), 1U);

  // The last item is [STYLE_OPTIONS] SIZE.
  std::vector<std::string> styles_size;
  base::SplitString(font_names->back(), ' ', &styles_size);
  DCHECK(!styles_size.empty());
  base::StringToInt(styles_size.back(), font_size);
  DCHECK_GT(*font_size, 0);
  font_names->pop_back();

  // Font supports BOLD and ITALIC; underline is supported via RenderText.
  *font_style = 0;
  for (size_t i = 0; i < styles_size.size() - 1; ++i) {
    // Styles are separated by white spaces. base::SplitString splits styles
    // by space, and it inserts empty string for continuous spaces.
    if (styles_size[i].empty())
      continue;
    if (!styles_size[i].compare("Bold"))
      *font_style |= gfx::Font::BOLD;
    else if (!styles_size[i].compare("Italic"))
      *font_style |= gfx::Font::ITALIC;
    else
      NOTREACHED();
  }
}

// Returns the font style and size as a string.
std::string FontStyleAndSizeToString(int font_style, int font_size) {
  std::string result;
  if (font_style & gfx::Font::BOLD)
    result += "Bold ";
  if (font_style & gfx::Font::ITALIC)
    result += "Italic ";
  result += base::IntToString(font_size);
  result += "px";
  return result;
}

// Returns font description from |font_names|, |font_style|, and |font_size|.
std::string BuildFontDescription(const std::vector<std::string>& font_names,
                                 int font_style,
                                 int font_size) {
  std::string description = JoinString(font_names, ',');
  description += "," + FontStyleAndSizeToString(font_style, font_size);
  return description;
}

}  // namespace

namespace gfx {

FontList::FontList()
    : common_height_(-1),
      common_baseline_(-1),
      font_style_(-1),
      font_size_(-1) {
  fonts_.push_back(Font());
}

FontList::FontList(const std::string& font_description_string)
    : font_description_string_(font_description_string),
      common_height_(-1),
      common_baseline_(-1),
      font_style_(-1),
      font_size_(-1) {
  DCHECK(!font_description_string.empty());
  // DCHECK description string ends with "px" for size in pixel.
  DCHECK(EndsWith(font_description_string, "px", true));
}

FontList::FontList(const std::vector<std::string>& font_names,
                   int font_style,
                   int font_size)
    : font_description_string_(BuildFontDescription(font_names, font_style,
                                                    font_size)),
      common_height_(-1),
      common_baseline_(-1),
      font_style_(font_style),
      font_size_(font_size) {
  DCHECK(!font_names.empty());
  DCHECK(!font_names[0].empty());
}

FontList::FontList(const std::vector<Font>& fonts)
    : fonts_(fonts),
      common_height_(-1),
      common_baseline_(-1),
      font_style_(-1),
      font_size_(-1) {
  DCHECK(!fonts.empty());
  font_style_ = fonts[0].GetStyle();
  font_size_ = fonts[0].GetFontSize();
  if (DCHECK_IS_ON()) {
    for (size_t i = 1; i < fonts.size(); ++i) {
      DCHECK_EQ(fonts[i].GetStyle(), font_style_);
      DCHECK_EQ(fonts[i].GetFontSize(), font_size_);
    }
  }
}

FontList::FontList(const Font& font)
    : common_height_(-1),
      common_baseline_(-1),
      font_style_(-1),
      font_size_(-1) {
  fonts_.push_back(font);
}

FontList::~FontList() {
}

FontList FontList::DeriveFontList(int font_style) const {
  return DeriveFontListWithSizeDeltaAndStyle(0, font_style);
}

FontList FontList::DeriveFontListWithSize(int size) const {
  DCHECK_GT(size, 0);
  return DeriveFontListWithSizeDeltaAndStyle(size - GetFontSize(),
                                             GetFontStyle());
}

FontList FontList::DeriveFontListWithSizeDelta(int size_delta) const {
  return DeriveFontListWithSizeDeltaAndStyle(size_delta, GetFontStyle());
}

FontList FontList::DeriveFontListWithSizeDeltaAndStyle(int size_delta,
                                                       int style) const {
  // If there is a font vector, derive from that.
  if (!fonts_.empty()) {
    std::vector<Font> fonts = fonts_;
    for (size_t i = 0; i < fonts.size(); ++i)
      fonts[i] = fonts[i].DeriveFont(size_delta, style);
    return FontList(fonts);
  }

  // Otherwise, parse the font description string to derive from it.
  std::vector<std::string> font_names;
  int old_size;
  int old_style;
  ParseFontDescriptionString(font_description_string_, &font_names,
                             &old_style, &old_size);
  int size = old_size + size_delta;
  DCHECK_GT(size, 0);
  return FontList(font_names, style, size);
}

int FontList::GetHeight() const {
  if (common_height_ == -1)
    CacheCommonFontHeightAndBaseline();
  return common_height_;
}

int FontList::GetBaseline() const {
  if (common_baseline_ == -1)
    CacheCommonFontHeightAndBaseline();
  return common_baseline_;
}

int FontList::GetCapHeight() const {
  // Assume the primary font is used to render Latin characters.
  return GetPrimaryFont().GetCapHeight();
}

int FontList::GetStringWidth(const base::string16& text) const {
  // Rely on the primary font metrics for the time being.
  // TODO(yukishiino): Not only the first font, all the fonts in the list should
  // be taken into account to compute the pixels needed to display |text|.
  // Also this method, including one in Font class, should be deprecated and
  // client code should call Canvas::GetStringWidth(text, font_list) directly.
  // Our plan is as follows:
  //   1. Introduce the FontList version of Canvas::GetStringWidth().
  //   2. Make client code call Canvas::GetStringWidth().
  //   3. Retire {Font,FontList}::GetStringWidth().
  return GetPrimaryFont().GetStringWidth(text);
}

int FontList::GetExpectedTextWidth(int length) const {
  // Rely on the primary font metrics for the time being.
  return GetPrimaryFont().GetExpectedTextWidth(length);
}

int FontList::GetFontStyle() const {
  if (font_style_ == -1)
    CacheFontStyleAndSize();
  return font_style_;
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
    font_description_string_ +=
        FontStyleAndSizeToString(fonts_[0].GetStyle(), fonts_[0].GetFontSize());
  }
  return font_description_string_;
}

int FontList::GetFontSize() const {
  if (font_size_ == -1)
    CacheFontStyleAndSize();
  return font_size_;
}

const std::vector<Font>& FontList::GetFonts() const {
  if (fonts_.empty()) {
    DCHECK(!font_description_string_.empty());

    std::vector<std::string> font_names;
    ParseFontDescriptionString(font_description_string_, &font_names,
                               &font_style_, &font_size_);
    for (size_t i = 0; i < font_names.size(); ++i) {
      DCHECK(!font_names[i].empty());

      Font font(font_names[i], font_size_);
      if (font_style_ == Font::NORMAL)
        fonts_.push_back(font);
      else
        fonts_.push_back(font.DeriveFont(0, font_style_));
    }
  }
  return fonts_;
}

const Font& FontList::GetPrimaryFont() const {
  return GetFonts()[0];
}

void FontList::CacheCommonFontHeightAndBaseline() const {
  int ascent = 0;
  int descent = 0;
  const std::vector<Font>& fonts = GetFonts();
  for (std::vector<Font>::const_iterator i = fonts.begin();
       i != fonts.end(); ++i) {
    ascent = std::max(ascent, i->GetBaseline());
    descent = std::max(descent, i->GetHeight() - i->GetBaseline());
  }
  common_height_ = ascent + descent;
  common_baseline_ = ascent;
}

void FontList::CacheFontStyleAndSize() const {
  if (!fonts_.empty()) {
    font_style_ = fonts_[0].GetStyle();
    font_size_ = fonts_[0].GetFontSize();
  } else {
    std::vector<std::string> font_names;
    ParseFontDescriptionString(font_description_string_, &font_names,
                               &font_style_, &font_size_);
  }
}

}  // namespace gfx
