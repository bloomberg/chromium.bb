// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_CANVAS_H_
#define UI_GFX_CANVAS_H_
#pragma once

#include <string>

#include "base/string16.h"
#include "gfx/native_widget_types.h"
// TODO(beng): remove this include when we no longer depend on SkTypes.
#include "skia/ext/platform_canvas.h"

namespace gfx {

class Brush;
class CanvasSkia;
class Font;
class Point;
class Rect;

// TODO(beng): documentation.
class Canvas {
 public:
  // Specifies the alignment for text rendered with the DrawStringInt method.
  enum {
    TEXT_ALIGN_LEFT = 1,
    TEXT_ALIGN_CENTER = 2,
    TEXT_ALIGN_RIGHT = 4,
    TEXT_VALIGN_TOP = 8,
    TEXT_VALIGN_MIDDLE = 16,
    TEXT_VALIGN_BOTTOM = 32,

    // Specifies the text consists of multiple lines.
    MULTI_LINE = 64,

    // By default DrawStringInt does not process the prefix ('&') character
    // specially. That is, the string "&foo" is rendered as "&foo". When
    // rendering text from a resource that uses the prefix character for
    // mnemonics, the prefix should be processed and can be rendered as an
    // underline (SHOW_PREFIX), or not rendered at all (HIDE_PREFIX).
    SHOW_PREFIX = 128,
    HIDE_PREFIX = 256,

    // Prevent ellipsizing
    NO_ELLIPSIS = 512,

    // Specifies if words can be split by new lines.
    // This only works with MULTI_LINE.
    CHARACTER_BREAK = 1024,

    // Instructs DrawStringInt() to render the text using RTL directionality.
    // In most cases, passing this flag is not necessary because information
    // about the text directionality is going to be embedded within the string
    // in the form of special Unicode characters. However, we don't insert
    // directionality characters into strings if the locale is LTR because some
    // platforms (for example, an English Windows XP with no RTL fonts
    // installed) don't support these characters. Thus, this flag should be
    // used to render text using RTL directionality when the locale is LTR.
    FORCE_RTL_DIRECTIONALITY = 2048,
  };

  virtual ~Canvas() {}

  // Creates an empty canvas. Must be initialized before it can be used.
  static Canvas* CreateCanvas();

  // Creates a canvas with the specified size.
  static Canvas* CreateCanvas(int width, int height, bool is_opaque);

  // Saves a copy of the drawing state onto a stack, operating on this copy
  // until a balanced call to Restore() is made.
  virtual void Save() = 0;

  // As with Save(), except draws to a layer that is blended with the canvas
  // at the specified alpha once Restore() is called.
  // |layer_bounds| are the bounds of the layer relative to the current
  // transform.
  virtual void SaveLayerAlpha(uint8 alpha) = 0;
  virtual void SaveLayerAlpha(uint8 alpha, const gfx::Rect& layer_bounds) = 0;

  // Restores the drawing state after a call to Save*(). It is an error to
  // call Restore() more times than Save*().
  virtual void Restore() = 0;

  // Wrapper function that takes integer arguments.
  // Returns true if the clip is non-empty.
  // See clipRect for specifics.
  virtual bool ClipRectInt(int x, int y, int w, int h) = 0;

  // Wrapper function that takes integer arguments.
  // See translate() for specifics.
  virtual void TranslateInt(int x, int y) = 0;

  // Wrapper function that takes integer arguments.
  // See scale() for specifics.
  virtual void ScaleInt(int x, int y) = 0;

  // Fills the specified region with the specified color using a transfer
  // mode of SkXfermode::kSrcOver_Mode.
  virtual void FillRectInt(const SkColor& color,
                           int x, int y, int w, int h) = 0;

  // Fills the specified region with the specified color and mode
  virtual void FillRectInt(const SkColor& color,
                           int x, int y, int w, int h,
                           SkXfermode::Mode mode) = 0;

  // Fills the specified region with the specified brush.
  virtual void FillRectInt(const gfx::Brush* brush,
                           int x, int y, int w, int h) = 0;

  // Draws a single pixel rect in the specified region with the specified
  // color, using a transfer mode of SkXfermode::kSrcOver_Mode.
  //
  // NOTE: if you need a single pixel line, use DrawLineInt.
  virtual void DrawRectInt(const SkColor& color,
                           int x, int y, int w, int h) = 0;

  // Draws a single pixel rect in the specified region with the specified
  // color and transfer mode.
  //
  // NOTE: if you need a single pixel line, use DrawLineInt.
  virtual void DrawRectInt(const SkColor& color,
                           int x, int y, int w, int h,
                           SkXfermode::Mode mode) = 0;

  // Draws the given rectangle with the given paint's parameters.
  virtual void DrawRectInt(int x, int y, int w, int h,
                           const SkPaint& paint) = 0;

  // Draws a single pixel line with the specified color.
  virtual void DrawLineInt(const SkColor& color,
                           int x1, int y1,
                           int x2, int y2) = 0;

  // Draws a bitmap with the origin at the specified location. The upper left
  // corner of the bitmap is rendered at the specified location.
  virtual void DrawBitmapInt(const SkBitmap& bitmap, int x, int y) = 0;

  // Draws a bitmap with the origin at the specified location, using the
  // specified paint. The upper left corner of the bitmap is rendered at the
  // specified location.
  virtual void DrawBitmapInt(const SkBitmap& bitmap,
                             int x, int y,
                             const SkPaint& paint) = 0;

  // Draws a portion of a bitmap in the specified location. The src parameters
  // correspond to the region of the bitmap to draw in the region defined
  // by the dest coordinates.
  //
  // If the width or height of the source differs from that of the destination,
  // the bitmap will be scaled. When scaling down, it is highly recommended
  // that you call buildMipMap(false) on your bitmap to ensure that it has
  // a mipmap, which will result in much higher-quality output. Set |filter|
  // to use filtering for bitmaps, otherwise the nearest-neighbor algorithm
  // is used for resampling.
  //
  // An optional custom SkPaint can be provided.
  virtual void DrawBitmapInt(const SkBitmap& bitmap,
                             int src_x, int src_y, int src_w, int src_h,
                             int dest_x, int dest_y, int dest_w, int dest_h,
                             bool filter) = 0;
  virtual void DrawBitmapInt(const SkBitmap& bitmap,
                             int src_x, int src_y, int src_w, int src_h,
                             int dest_x, int dest_y, int dest_w, int dest_h,
                             bool filter,
                             const SkPaint& paint) = 0;

  // Draws text with the specified color, font and location. The text is
  // aligned to the left, vertically centered, clipped to the region. If the
  // text is too big, it is truncated and '...' is added to the end.
  virtual void DrawStringInt(const string16& text, const
                             gfx::Font& font,
                             const SkColor& color,
                             int x, int y, int w, int h) = 0;
  virtual void DrawStringInt(const string16& text,
                             const gfx::Font& font,
                             const SkColor& color,
                             const gfx::Rect& display_rect) = 0;

  // Draws text with the specified color, font and location. The last argument
  // specifies flags for how the text should be rendered. It can be one of
  // TEXT_ALIGN_CENTER, TEXT_ALIGN_RIGHT or TEXT_ALIGN_LEFT.
  virtual void DrawStringInt(const string16& text,
                             const gfx::Font& font,
                             const SkColor& color,
                             int x, int y, int w, int h,
                             int flags) = 0;

  // Draws a dotted gray rectangle used for focus purposes.
  virtual void DrawFocusRect(int x, int y, int width, int height) = 0;

  // Tiles the image in the specified region.
  virtual void TileImageInt(const SkBitmap& bitmap,
                            int x, int y, int w, int h) = 0;
  virtual void TileImageInt(const SkBitmap& bitmap,
                            int src_x, int src_y,
                            int dest_x, int dest_y, int w, int h) = 0;

  // Returns a native drawing context for platform specific drawing routines to
  // use. Must be balanced by a call to EndPlatformPaint().
  virtual gfx::NativeDrawingContext BeginPlatformPaint() = 0;

  // Signifies the end of platform drawing using the native drawing context
  // returned by BeginPlatformPaint().
  virtual void EndPlatformPaint() = 0;

  // TODO(beng): remove this once we don't need to use any skia-specific methods
  //             through this interface.
  // A quick and dirty way to obtain the underlying SkCanvas.
  virtual CanvasSkia* AsCanvasSkia();
  virtual const CanvasSkia* AsCanvasSkia() const;
};

class CanvasPaint {
 public:
  virtual ~CanvasPaint() {}

  // Creates a canvas that paints to |view| when it is destroyed. The canvas is
  // sized to the client area of |view|.
  static CanvasPaint* CreateCanvasPaint(gfx::NativeView view);

  // Returns true if the canvas has an invalid rect that needs to be repainted.
  virtual bool IsValid() const = 0;

  // Returns the rectangle that is invalid.
  virtual gfx::Rect GetInvalidRect() const = 0;

  // Returns the underlying Canvas.
  virtual Canvas* AsCanvas() = 0;
};

}  // namespace gfx;

#endif  // UI_GFX_CANVAS_H_
