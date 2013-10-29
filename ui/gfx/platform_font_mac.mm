// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/platform_font_mac.h"

#include <Cocoa/Cocoa.h>

#include "base/basictypes.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"

namespace gfx {

////////////////////////////////////////////////////////////////////////////////
// PlatformFontMac, public:

PlatformFontMac::PlatformFontMac()
    : native_font_([[NSFont systemFontOfSize:[NSFont systemFontSize]] retain]) {
  InitAndCalculateMetrics();
}

PlatformFontMac::PlatformFontMac(NativeFont native_font)
    : native_font_([native_font retain]) {
  InitAndCalculateMetrics();
}

PlatformFontMac::PlatformFontMac(const std::string& font_name,
                                 int font_size) {
  native_font_.reset([[NSFont fontWithName:base::SysUTF8ToNSString(font_name)
                                      size:font_size] retain]);
  InitAndCalculateMetrics();
}

////////////////////////////////////////////////////////////////////////////////
// PlatformFontMac, PlatformFont implementation:

Font PlatformFontMac::DeriveFont(int size_delta, int style) const {
  NSFont* font = native_font_.get();
  NSFontManager* font_manager = [NSFontManager sharedFontManager];

  if (size_delta) {
    font = [font_manager convertFont:font
                              toSize:font_size_ + size_delta];
  }
  if (style & Font::BOLD) {
    font = [font_manager convertFont:font
                         toHaveTrait:NSBoldFontMask];
  } else {
    font = [font_manager convertFont:font
                      toNotHaveTrait:NSBoldFontMask];
  }
  if (style & Font::ITALIC) {
    font = [font_manager convertFont:font
                         toHaveTrait:NSItalicFontMask];
  } else {
    font = [font_manager convertFont:font
                      toNotHaveTrait:NSItalicFontMask];
  }
  // The Mac doesn't support underline as a font trait, so just drop it.
  // Underlines can instead be added as an attribute on an NSAttributedString.

  return Font(new PlatformFontMac(font));
}

int PlatformFontMac::GetHeight() const {
  return height_;
}

int PlatformFontMac::GetBaseline() const {
  return ascent_;
}

int PlatformFontMac::GetCapHeight() const {
  return cap_height_;
}

int PlatformFontMac::GetAverageCharacterWidth() const {
  return average_width_;
}

int PlatformFontMac::GetStringWidth(const base::string16& text) const {
  return Canvas::GetStringWidth(text,
                                Font(const_cast<PlatformFontMac*>(this)));
}

int PlatformFontMac::GetExpectedTextWidth(int length) const {
  return length * average_width_;
}

int PlatformFontMac::GetStyle() const {
  return style_;
}

std::string PlatformFontMac::GetFontName() const {
  return font_name_;
}

int PlatformFontMac::GetFontSize() const {
  return font_size_;
}

NativeFont PlatformFontMac::GetNativeFont() const {
  return native_font_.get();
}

////////////////////////////////////////////////////////////////////////////////
// PlatformFontMac, private:

PlatformFontMac::~PlatformFontMac() {
}

void PlatformFontMac::InitAndCalculateMetrics() {
  NSFont* font = native_font_.get();

  font_name_ = base::SysNSStringToUTF8([font familyName]);
  font_size_ = [font pointSize];
  style_ = 0;
  NSFontSymbolicTraits traits = [[font fontDescriptor] symbolicTraits];
  if (traits & NSFontItalicTrait)
    style_ |= Font::ITALIC;
  if (traits & NSFontBoldTrait)
    style_ |= Font::BOLD;

  base::scoped_nsobject<NSLayoutManager> layout_manager(
      [[NSLayoutManager alloc] init]);
  height_ = [layout_manager defaultLineHeightForFont:font];
  ascent_ = [font ascender];
  cap_height_ = [font capHeight];
  average_width_ =
      NSWidth([font boundingRectForGlyph:[font glyphWithName:@"x"]]);
}

////////////////////////////////////////////////////////////////////////////////
// PlatformFont, public:

// static
PlatformFont* PlatformFont::CreateDefault() {
  return new PlatformFontMac;
}

// static
PlatformFont* PlatformFont::CreateFromNativeFont(NativeFont native_font) {
  return new PlatformFontMac(native_font);
}

// static
PlatformFont* PlatformFont::CreateFromNameAndSize(const std::string& font_name,
                                                  int font_size) {
  return new PlatformFontMac(font_name, font_size);
}

}  // namespace gfx
