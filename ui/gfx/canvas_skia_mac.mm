// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "ui/gfx/canvas_skia.h"

#include "base/logging.h"
#include "base/sys_string_conversions.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "ui/gfx/font.h"

// Note: This is a temporary Skia-based implementation of the ui/gfx text
// rendering routines for views/aura.  It replaces the stale Cocoa-based
// implementation.  A future |canvas_skia_skia.cc| implementation will supersede
// this and the other platform-specific implmenentations.
// Most drawing options, such as alignment and multi-line, are not implemented
// here.

namespace {

SkTypeface::Style FontTypefaceStyle(const gfx::Font& font) {
  int style = 0;
  if (font.GetStyle() & gfx::Font::BOLD)
    style |= SkTypeface::kBold;
  if (font.GetStyle() & gfx::Font::ITALIC)
    style |= SkTypeface::kItalic;

  return static_cast<SkTypeface::Style>(style);
}

}  // namespace

namespace gfx {

// static
void CanvasSkia::SizeStringInt(const string16& text,
                               const gfx::Font& font,
                               int* width,
                               int* height,
                               int flags) {
  NSFont* native_font = font.GetNativeFont();
  NSString* ns_string = base::SysUTF16ToNSString(text);
  NSDictionary* attributes =
      [NSDictionary dictionaryWithObject:native_font
                                  forKey:NSFontAttributeName];
  NSSize string_size = [ns_string sizeWithAttributes:attributes];
  *width = string_size.width;
  *height = font.GetHeight();
}

void CanvasSkia::DrawStringInt(const string16& text,
                               const gfx::Font& font,
                               const SkColor& color,
                               int x, int y, int w, int h,
                               int flags) {
  SkTypeface* typeface = SkTypeface::CreateFromName(font.GetFontName().c_str(),
                                                    FontTypefaceStyle(font));
  SkPaint paint;
  paint.setTypeface(typeface);
  typeface->unref();
  paint.setColor(color);
  canvas_->drawText(text.c_str(),
                    text.size() * sizeof(string16::value_type),
                    x,
                    y + h,
                    paint);
}

void CanvasSkia::DrawStringWithHalo(const string16& text,
                                    const gfx::Font& font,
                                    const SkColor& text_color,
                                    const SkColor& halo_color,
                                    int x, int y, int w, int h,
                                    int flags) {
}

ui::TextureID CanvasSkia::GetTextureID() {
  return 0;
}

}  // namespace gfx
