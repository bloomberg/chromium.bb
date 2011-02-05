// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/platform_font_mac.h"

#include <Cocoa/Cocoa.h>

#include "base/basictypes.h"
#include "base/scoped_nsobject.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/font.h"

namespace gfx {

////////////////////////////////////////////////////////////////////////////////
// PlatformFontMac, public:

PlatformFontMac::PlatformFontMac() {
  font_size_ = [NSFont systemFontSize];
  style_ = gfx::Font::NORMAL;
  NSFont* system_font = [NSFont systemFontOfSize:font_size_];
  font_name_ = base::SysNSStringToUTF16([system_font fontName]);
  CalculateMetrics();
}

PlatformFontMac::PlatformFontMac(const Font& other) {
}

PlatformFontMac::PlatformFontMac(NativeFont native_font) {
}

PlatformFontMac::PlatformFontMac(const string16& font_name,
                                 int font_size) {
  InitWithNameSizeAndStyle(font_name, font_size, gfx::Font::NORMAL);
}

////////////////////////////////////////////////////////////////////////////////
// PlatformFontMac, PlatformFont implementation:

Font PlatformFontMac::DeriveFont(int size_delta, int style) const {
  return Font(new PlatformFontMac(font_name_, font_size_ + size_delta, style));
}

int PlatformFontMac::GetHeight() const {
  return height_;
}

int PlatformFontMac::GetBaseline() const {
  return ascent_;
}

int PlatformFontMac::GetAverageCharacterWidth() const {
  return average_width_;
}

int PlatformFontMac::GetStringWidth(const string16& text) const {
  int width = 0, height = 0;
  CanvasSkia::SizeStringInt(text, Font(const_cast<PlatformFontMac*>(this)),
                            &width, &height, gfx::Canvas::NO_ELLIPSIS);
  return width;
}

int PlatformFontMac::GetExpectedTextWidth(int length) const {
  return length * average_width_;
}

int PlatformFontMac::GetStyle() const {
  return style_;
}

string16 PlatformFontMac::GetFontName() const {
  return font_name_;
}

int PlatformFontMac::GetFontSize() const {
  return font_size_;
}

NativeFont PlatformFontMac::GetNativeFont() const {
  // TODO(pinkerton): apply |style_| to font. http://crbug.com/34667
  // We could cache this, but then we'd have to conditionally change the
  // dtor just for MacOS. Not sure if we want to/need to do that.
  return [NSFont fontWithName:base::SysUTF16ToNSString(font_name_)
                         size:font_size_];
}

////////////////////////////////////////////////////////////////////////////////
// PlatformFontMac, private:

PlatformFontMac::PlatformFontMac(const string16& font_name,
                                 int font_size,
                                 int style) {
  InitWithNameSizeAndStyle(font_name, font_size, style);
}

void PlatformFontMac::InitWithNameSizeAndStyle(const string16& font_name,
                                               int font_size,
                                               int style) {
  font_name_ = font_name;
  font_size_ = font_size;
  style_ = style;
  CalculateMetrics();
}

void PlatformFontMac::CalculateMetrics() {
  NSFont* font = GetNativeFont();
  scoped_nsobject<NSLayoutManager> layout_manager(
      [[NSLayoutManager alloc] init]);
  height_ = [layout_manager defaultLineHeightForFont:font];
  ascent_ = [font ascender];
  average_width_ =
      [font boundingRectForGlyph:[font glyphWithName:@"x"]].size.width;
}

////////////////////////////////////////////////////////////////////////////////
// PlatformFont, public:

// static
PlatformFont* PlatformFont::CreateDefault() {
  return new PlatformFontMac;
}

// static
PlatformFont* PlatformFont::CreateFromFont(const Font& other) {
  return new PlatformFontMac(other);
}

// static
PlatformFont* PlatformFont::CreateFromNativeFont(NativeFont native_font) {
  return new PlatformFontMac(native_font);
}

// static
PlatformFont* PlatformFont::CreateFromNameAndSize(const string16& font_name,
                                                  int font_size) {
  return new PlatformFontMac(font_name, font_size);
}

}  // namespace gfx

