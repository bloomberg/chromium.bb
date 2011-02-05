// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_FONT_H_
#define UI_GFX_FONT_H_
#pragma once

#include <string>

#include "base/ref_counted.h"
#include "base/string16.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {

class PlatformFont;

// Font provides a wrapper around an underlying font. Copy and assignment
// operators are explicitly allowed, and cheap.
class Font {
 public:
  // The following constants indicate the font style.
  enum FontStyle {
    NORMAL = 0,
    BOLD = 1,
    ITALIC = 2,
    UNDERLINED = 4,
  };

  // Creates a font with the default name and style.
  Font();

  // Creates a font that is a clone of another font object.
  Font(const Font& other);
  gfx::Font& operator=(const Font& other);

  // Creates a font from the specified native font.
  explicit Font(NativeFont native_font);

  // Construct a Font object with the specified PlatformFont object. The Font
  // object takes ownership of the PlatformFont object.
  explicit Font(PlatformFont* platform_font);

  // Creates a font with the specified name and size.
  Font(const string16& font_name, int font_size);

  ~Font();

  // Returns a new Font derived from the existing font.
  // size_deta is the size to add to the current font. For example, a value
  // of 5 results in a font 5 units bigger than this font.
  Font DeriveFont(int size_delta) const;

  // Returns a new Font derived from the existing font.
  // size_delta is the size to add to the current font. See the single
  // argument version of this method for an example.
  // The style parameter specifies the new style for the font, and is a
  // bitmask of the values: BOLD, ITALIC and UNDERLINED.
  Font DeriveFont(int size_delta, int style) const;

  // Returns the number of vertical pixels needed to display characters from
  // the specified font.  This may include some leading, i.e. height may be
  // greater than just ascent + descent.  Specifically, the Windows and Mac
  // implementations include leading and the Linux one does not.  This may
  // need to be revisited in the future.
  int GetHeight() const;

  // Returns the baseline, or ascent, of the font.
  int GetBaseline() const;

  // Returns the average character width for the font.
  int GetAverageCharacterWidth() const;

  // Returns the number of horizontal pixels needed to display the specified
  // string.
  int GetStringWidth(const string16& text) const;

  // Returns the expected number of horizontal pixels needed to display the
  // specified length of characters. Call GetStringWidth() to retrieve the
  // actual number.
  int GetExpectedTextWidth(int length) const;

  // Returns the style of the font.
  int GetStyle() const;

  // Returns the font name.
  string16 GetFontName() const;

  // Returns the font size in pixels.
  int GetFontSize() const;

  // Returns the native font handle.
  // Lifetime lore:
  // Windows: This handle is owned by the Font object, and should not be
  //          destroyed by the caller.
  // Mac:     Caller must release this object.
  // Gtk:     This handle is created on demand, and must be freed by calling
  //          pango_font_description_free() when the caller is done using it.
  NativeFont GetNativeFont() const;

  // Raw access to the underlying platform font implementation. Can be
  // static_cast to a known implementation type if needed.
  PlatformFont* platform_font() const { return platform_font_.get(); }

 private:
  // Wrapped platform font implementation.
  scoped_refptr<PlatformFont> platform_font_;
};

}  // namespace gfx

#endif  // UI_GFX_FONT_H_
