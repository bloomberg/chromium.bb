// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "ui/gfx/canvas_skia.h"

#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/sys_string_conversions.h"
#include "third_party/skia/include/core/SkShader.h"
#include "ui/gfx/font.h"
#include "ui/gfx/rect.h"

namespace gfx {

CanvasSkia::CanvasSkia(int width, int height, bool is_opaque)
    : skia::PlatformCanvas(width, height, is_opaque) {
}

CanvasSkia::CanvasSkia() : skia::PlatformCanvas() {
}

CanvasSkia::~CanvasSkia() {
}

// static
void CanvasSkia::SizeStringInt(const string16& text,
                               const gfx::Font& font,
                               int* width, int* height,
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
  if (!IntersectsClipRectInt(x, y, w, h))
    return;

  CGContextRef context = beginPlatformPaint();
  CGContextSaveGState(context);

  NSColor* ns_color = [NSColor colorWithDeviceRed:SkColorGetR(color) / 255.0
                                            green:SkColorGetG(color) / 255.0
                                             blue:SkColorGetB(color) / 255.0
                                            alpha:SkColorGetA(color) / 255.0];
  NSMutableParagraphStyle *ns_style =
      [[[NSParagraphStyle alloc] init] autorelease];
  if (flags & TEXT_ALIGN_CENTER)
    [ns_style setAlignment:NSCenterTextAlignment];
  // TODO(awalker): Implement the rest of the Canvas text flags

  NSDictionary* attributes =
      [NSDictionary dictionaryWithObjectsAndKeys:
          font.GetNativeFont(), NSFontAttributeName,
          ns_color, NSForegroundColorAttributeName,
          ns_style, NSParagraphStyleAttributeName,
          nil];

  NSAttributedString* ns_string =
      [[[NSAttributedString alloc] initWithString:base::SysUTF16ToNSString(text)
                                       attributes:attributes] autorelease];
  base::mac::ScopedCFTypeRef<CTFramesetterRef> framesetter(
      CTFramesetterCreateWithAttributedString(
          base::mac::NSToCFCast(ns_string)));

  CGRect text_bounds = CGRectMake(x, y, w, h);
  CGMutablePathRef path = CGPathCreateMutable();
  CGPathAddRect(path, NULL, text_bounds);

  base::mac::ScopedCFTypeRef<CTFrameRef> frame(
      CTFramesetterCreateFrame(framesetter, CFRangeMake(0, 0), path, NULL));
  CTFrameDraw(frame, context);
  CGContextRestoreGState(context);
  endPlatformPaint();
}

}  // namespace gfx
