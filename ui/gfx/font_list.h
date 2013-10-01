// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_FONT_LIST_H_
#define UI_GFX_FONT_LIST_H_

#include <string>
#include <vector>

#include "ui/gfx/font.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

// FontList represents a list of fonts either in the form of Font vector or in
// the form of a string representing font names, styles, and size.
//
// The string representation is in the form "FAMILY_LIST [STYLE_OPTIONS] SIZE",
// where FAMILY_LIST is a comma separated list of families terminated by a
// comma, STYLE_OPTIONS is a whitespace separated list of words where each word
// describes one of style, variant, weight, stretch, or gravity, and SIZE is
// a decimal number followed by "px" for absolute size. STYLE_OPTIONS may be
// absent.
//
// The string format complies with that of Pango detailed at
// http://developer.gnome.org/pango/stable/pango-Fonts.html#pango-font-description-from-string
//
// FontList could be initialized either way without conversion to the other
// form. The conversion to the other form is done only when asked to get the
// other form.
//
// FontList allows operator= since FontList is a data member type in RenderText,
// and operator= is used in RenderText::SetFontList().
class GFX_EXPORT FontList {
 public:
  // Creates a font list with a Font with default name and style.
  FontList();

  // Creates a font list from a string representing font names, styles, and
  // size.
  explicit FontList(const std::string& font_description_string);

  // Creates a font list from font names, styles and size.
  FontList(const std::vector<std::string>& font_names,
           int font_style,
           int font_size);

  // Creates a font list from a Font vector.
  // All fonts in this vector should have the same style and size.
  explicit FontList(const std::vector<Font>& fonts);

  // Creates a font list from a Font.
  explicit FontList(const Font& font);

  ~FontList();

  // Returns a new FontList with the given |font_style| flags.
  FontList DeriveFontList(int font_style) const;

  // Returns a new FontList with the same font names and style but with the
  // given font |size| in pixels.
  FontList DeriveFontListWithSize(int size) const;

  // Returns a new FontList with the same font names and style but resized.
  // |size_delta| is the size in pixels to add to the current font size.
  FontList DeriveFontListWithSizeDelta(int size_delta) const;

  // Returns a new FontList with the same font names but resized and the given
  // style. |size_delta| is the size in pixels to add to the current font size.
  // |font_style| specifies the new style, which is a bitmask of the values:
  // Font::BOLD, Font::ITALIC and Font::UNDERLINE.
  FontList DeriveFontListWithSizeDeltaAndStyle(int size_delta,
                                               int font_style) const;

  // Returns the height of this font list, which is max(ascent) + max(descent)
  // for all the fonts in the font list.
  int GetHeight() const;

  // Returns the baseline of this font list, which is max(baseline) for all the
  // fonts in the font list.
  int GetBaseline() const;

  // Returns the cap height of this font list.
  // Currently returns the cap height of the primary font.
  int GetCapHeight() const;

  // Returns the number of horizontal pixels needed to display |text|.
  int GetStringWidth(const base::string16& text) const;

  // Returns the expected number of horizontal pixels needed to display the
  // specified length of characters. Call GetStringWidth() to retrieve the
  // actual number.
  int GetExpectedTextWidth(int length) const;

  // Returns the |gfx::Font::FontStyle| style flags for this font list.
  int GetFontStyle() const;

  // Returns a string representing font names, styles, and size. If the FontList
  // is initialized by a vector of Font, use the first font's style and size
  // for the description.
  const std::string& GetFontDescriptionString() const;

  // Returns the font size in pixels.
  int GetFontSize() const;

  // Returns the Font vector.
  const std::vector<Font>& GetFonts() const;

  // Returns the first font in the list.
  const Font& GetPrimaryFont() const;

 private:
  // Extracts common font height and baseline into |common_height_| and
  // |common_baseline_|.
  void CacheCommonFontHeightAndBaseline() const;

  // Extracts font style and size into |font_style_| and |font_size_|.
  void CacheFontStyleAndSize() const;

  // A vector of Font. If FontList is constructed with font description string,
  // |fonts_| is not initialized during construction. Instead, it is computed
  // lazily when user asked to get the font vector.
  mutable std::vector<Font> fonts_;

  // A string representing font names, styles, and sizes.
  // Please refer to the comments before class declaration for details on string
  // format.
  // If FontList is constructed with a vector of font,
  // |font_description_string_| is not initialized during construction. Instead,
  // it is computed lazily when user asked to get the font description string.
  mutable std::string font_description_string_;

  // The cached common height and baseline of the fonts in the font list.
  mutable int common_height_;
  mutable int common_baseline_;

  // Cached font style and size.
  mutable int font_style_;
  mutable int font_size_;
};

}  // namespace gfx

#endif  // UI_GFX_FONT_LIST_H_
