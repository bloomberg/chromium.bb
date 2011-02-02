// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_PLATFORM_FONT_
#define UI_GFX_PLATFORM_FONT_
#pragma once

#include <string>

#include "base/ref_counted.h"
#include "base/string16.h"
#include "gfx/native_widget_types.h"

namespace gfx {

class Font;

class PlatformFont : public base::RefCounted<PlatformFont> {
 public:
  // Create an appropriate PlatformFont implementation.
  static PlatformFont* CreateDefault();
  static PlatformFont* CreateFromFont(const Font& other);
  static PlatformFont* CreateFromNativeFont(NativeFont native_font);
  static PlatformFont* CreateFromNameAndSize(const string16& font_name,
                                             int font_size);

  // Returns a new Font derived from the existing font.
  // size_delta is the size to add to the current font. See the single
  // argument version of this method for an example.
  // The style parameter specifies the new style for the font, and is a
  // bitmask of the values: BOLD, ITALIC and UNDERLINED.
  virtual Font DeriveFont(int size_delta, int style) const = 0;

  // Returns the number of vertical pixels needed to display characters from
  // the specified font.  This may include some leading, i.e. height may be
  // greater than just ascent + descent.  Specifically, the Windows and Mac
  // implementations include leading and the Linux one does not.  This may
  // need to be revisited in the future.
  virtual int GetHeight() const = 0;

  // Returns the baseline, or ascent, of the font.
  virtual int GetBaseline() const = 0;

  // Returns the average character width for the font.
  virtual int GetAverageCharacterWidth() const = 0;

  // Returns the number of horizontal pixels needed to display the specified
  // string.
  virtual int GetStringWidth(const string16& text) const = 0;

  // Returns the expected number of horizontal pixels needed to display the
  // specified length of characters. Call GetStringWidth() to retrieve the
  // actual number.
  virtual int GetExpectedTextWidth(int length) const = 0;

  // Returns the style of the font.
  virtual int GetStyle() const = 0;

  // Returns the font name.
  virtual string16 GetFontName() const = 0;

  // Returns the font size in pixels.
  virtual int GetFontSize() const = 0;

  // Returns the native font handle.
  virtual NativeFont GetNativeFont() const = 0;

 protected:
  virtual ~PlatformFont() {}

 private:
  friend class base::RefCounted<PlatformFont>;
};

}  // namespace gfx

#endif  // UI_GFX_PLATFORM_FONT_

