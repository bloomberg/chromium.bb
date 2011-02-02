// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gfx/font.h"

#include "base/utf_string_conversions.h"
#include "gfx/platform_font.h"

namespace gfx {

////////////////////////////////////////////////////////////////////////////////
// Font, public:

Font::Font() : platform_font_(PlatformFont::CreateDefault()) {
}

Font::Font(const Font& other) : platform_font_(other.platform_font_) {
}

gfx::Font& Font::operator=(const Font& other) {
  platform_font_ = other.platform_font_;
  return *this;
}

Font::Font(NativeFont native_font)
    : platform_font_(PlatformFont::CreateFromNativeFont(native_font)) {
}

Font::Font(PlatformFont* platform_font) : platform_font_(platform_font) {
}

Font::Font(const string16& font_name, int font_size)
    : platform_font_(PlatformFont::CreateFromNameAndSize(font_name,
                                                         font_size)) {
}

Font::~Font() {
}

Font Font::DeriveFont(int size_delta) const {
  return DeriveFont(size_delta, GetStyle());
}

Font Font::DeriveFont(int size_delta, int style) const {
  return platform_font_->DeriveFont(size_delta, style);
}

int Font::GetHeight() const {
  return platform_font_->GetHeight();
}

int Font::GetBaseline() const {
  return platform_font_->GetBaseline();
}

int Font::GetAverageCharacterWidth() const {
  return platform_font_->GetAverageCharacterWidth();
}

int Font::GetStringWidth(const string16& text) const {
  return platform_font_->GetStringWidth(text);
}

int Font::GetExpectedTextWidth(int length) const {
  return platform_font_->GetExpectedTextWidth(length);
}

int Font::GetStyle() const {
  return platform_font_->GetStyle();
}

string16 Font::GetFontName() const {
  return platform_font_->GetFontName();
}

int Font::GetFontSize() const {
  return platform_font_->GetFontSize();
}

NativeFont Font::GetNativeFont() const {
  return platform_font_->GetNativeFont();
}

}  // namespace gfx
