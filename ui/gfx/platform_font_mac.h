// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_PLATFORM_FONT_MAC_
#define UI_GFX_PLATFORM_FONT_MAC_
#pragma once

#include "gfx/platform_font.h"

namespace gfx {

class PlatformFontMac : public PlatformFont {
 public:
  PlatformFontMac();
  explicit PlatformFontMac(const Font& other);
  explicit PlatformFontMac(NativeFont native_font);
  PlatformFontMac(const string16& font_name,
                  int font_size);

  // Overridden from PlatformFont:
  virtual Font DeriveFont(int size_delta, int style) const;
  virtual int GetHeight() const;
  virtual int GetBaseline() const;
  virtual int GetAverageCharacterWidth() const;
  virtual int GetStringWidth(const string16& text) const;
  virtual int GetExpectedTextWidth(int length) const;
  virtual int GetStyle() const;
  virtual string16 GetFontName() const;
  virtual int GetFontSize() const;
  virtual NativeFont GetNativeFont() const;

 private:
  PlatformFontMac(const string16& font_name, int font_size, int style);
  virtual ~PlatformFontMac() {}

  // Initialize the object with the specified parameters.
  void InitWithNameSizeAndStyle(const string16& font_name,
                                int font_size,
                                int style);

  // Calculate and cache the font metrics.
  void CalculateMetrics();

  string16 font_name_;
  int font_size_;
  int style_;

  // Cached metrics, generated at construction
  int height_;
  int ascent_;
  int average_width_;
};

}  // namespace gfx

#endif  // UI_GFX_PLATFORM_FONT_MAC_
