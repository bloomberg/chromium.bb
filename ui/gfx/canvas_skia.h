// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_CANVAS_SKIA_H_
#define UI_GFX_CANVAS_SKIA_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "skia/ext/platform_canvas.h"
#include "ui/gfx/canvas.h"

#if defined(TOOLKIT_USES_GTK)
typedef struct _GdkPixbuf GdkPixbuf;
#endif

namespace gfx {

class Canvas;

// CanvasSkia is a SkCanvas subclass that provides a number of methods for
// common operations used throughout an application built using base/gfx and
// app/gfx.
//
// All methods that take integer arguments (as is used throughout views)
// end with Int. If you need to use methods provided by the superclass
// you'll need to do a conversion. In particular you'll need to use
// macro SkIntToScalar(xxx), or if converting from a scalar to an integer
// SkScalarRound.
//
// A handful of methods in this class are overloaded providing an additional
// argument of type SkXfermode::Mode. SkXfermode::Mode specifies how the
// source and destination colors are combined. Unless otherwise specified,
// the variant that does not take a SkXfermode::Mode uses a transfer mode
// of kSrcOver_Mode.
class UI_EXPORT CanvasSkia : public Canvas {
 public:
  enum TruncateFadeMode {
    TruncateFadeTail,
    TruncateFadeHead,
    TruncateFadeHeadAndTail,
  };

  // Creates an empty Canvas. Callers must use initialize before using the
  // canvas.
  CanvasSkia();

  CanvasSkia(int width, int height, bool is_opaque);
  explicit CanvasSkia(SkCanvas* canvas);

  virtual ~CanvasSkia();

  // Compute the size required to draw some text with the provided font.
  // Attempts to fit the text with the provided width and height. Increases
  // height and then width as needed to make the text fit. This method
  // supports multiple lines.
  static void SizeStringInt(const string16& text,
                            const gfx::Font& font,
                            int* width, int* height,
                            int flags);

  // Returns the default text alignment to be used when drawing text on a
  // gfx::CanvasSkia based on the directionality of the system locale language.
  // This function is used by gfx::Canvas::DrawStringInt when the text alignment
  // is not specified.
  //
  // This function returns either gfx::Canvas::TEXT_ALIGN_LEFT or
  // gfx::Canvas::TEXT_ALIGN_RIGHT.
  static int DefaultCanvasTextAlignment();

#if defined(TOOLKIT_USES_GTK)
  // Draw the pixbuf in its natural size at (x, y).
  void DrawGdkPixbuf(GdkPixbuf* pixbuf, int x, int y);
#endif

#if defined(OS_WIN) || (defined(OS_POSIX) && !defined(OS_MACOSX))
  // Draws text with a 1-pixel halo around it of the given color.
  // On Windows, it allows ClearType to be drawn to an otherwise transparenct
  //   bitmap for drag images. Drag images have only 1-bit of transparency, so
  //   we don't do any fancy blurring.
  // On Linux, text with halo is created by stroking it with 2px |halo_color|
  //   then filling it with |text_color|.
  void DrawStringWithHalo(const string16& text,
                          const gfx::Font& font,
                          const SkColor& text_color,
                          const SkColor& halo_color,
                          int x, int y, int w, int h,
                          int flags);
#endif

  // Extracts a bitmap from the contents of this canvas.
  SkBitmap ExtractBitmap() const;

  // Overridden from Canvas:
  virtual void Save() OVERRIDE;
  virtual void SaveLayerAlpha(uint8 alpha) OVERRIDE;
  virtual void SaveLayerAlpha(uint8 alpha,
                              const gfx::Rect& layer_bounds) OVERRIDE;
  virtual void Restore() OVERRIDE;
  virtual bool ClipRect(const gfx::Rect& rect) OVERRIDE;
  virtual void Translate(const gfx::Point& point) OVERRIDE;
  virtual void Scale(int x_scale, int y_scale) OVERRIDE;
  virtual void FillRect(const SkColor& color, const gfx::Rect& rect) OVERRIDE;
  virtual void FillRect(const SkColor& color,
                        const gfx::Rect& rect,
                        SkXfermode::Mode mode) OVERRIDE;
  virtual void FillRect(const gfx::Brush* brush,
                        const gfx::Rect& rect) OVERRIDE;
  virtual void DrawRect(const gfx::Rect& rect, const SkColor& color) OVERRIDE;
  virtual void DrawRect(const gfx::Rect& rect,
                        const SkColor& color,
                        SkXfermode::Mode mode) OVERRIDE;
  virtual void DrawRect(const gfx::Rect& rect, const SkPaint& paint) OVERRIDE;
  virtual void DrawLineInt(const SkColor& color,
                           int x1, int y1,
                           int x2, int y2) OVERRIDE;
  virtual void DrawBitmapInt(const SkBitmap& bitmap, int x, int y) OVERRIDE;
  virtual void DrawBitmapInt(const SkBitmap& bitmap,
                             int x, int y,
                             const SkPaint& paint) OVERRIDE;
  virtual void DrawBitmapInt(const SkBitmap& bitmap,
                             int src_x, int src_y, int src_w, int src_h,
                             int dest_x, int dest_y, int dest_w, int dest_h,
                             bool filter) OVERRIDE;
  virtual void DrawBitmapInt(const SkBitmap& bitmap,
                             int src_x, int src_y, int src_w, int src_h,
                             int dest_x, int dest_y, int dest_w, int dest_h,
                             bool filter,
                             const SkPaint& paint) OVERRIDE;
  virtual void DrawStringInt(const string16& text,
                             const gfx::Font& font,
                             const SkColor& color,
                             int x, int y, int w, int h) OVERRIDE;
  virtual void DrawStringInt(const string16& text,
                             const gfx::Font& font,
                             const SkColor& color,
                             const gfx::Rect& display_rect) OVERRIDE;
  virtual void DrawStringInt(const string16& text,
                             const gfx::Font& font,
                             const SkColor& color,
                             int x, int y, int w, int h,
                             int flags) OVERRIDE;
#if defined(OS_WIN)
  // Draws the given string with the beginning and/or the end using a fade
  // gradient. When truncating the head
  // |desired_characters_to_truncate_from_head| specifies the maximum number of
  // characters that can be truncated.
  virtual void DrawFadeTruncatingString(
      const string16& text,
      TruncateFadeMode truncate_mode,
      size_t desired_characters_to_truncate_from_head,
      const gfx::Font& font,
      const SkColor& color,
      const gfx::Rect& display_rect);
#endif
  virtual void DrawFocusRect(const gfx::Rect& rect) OVERRIDE;
  virtual void TileImageInt(const SkBitmap& bitmap,
                            int x, int y, int w, int h) OVERRIDE;
  virtual void TileImageInt(const SkBitmap& bitmap,
                            int src_x, int src_y,
                            int dest_x, int dest_y, int w, int h) OVERRIDE;
  virtual gfx::NativeDrawingContext BeginPlatformPaint() OVERRIDE;
  virtual void EndPlatformPaint() OVERRIDE;
#if !defined(OS_MACOSX)
  virtual void Transform(const ui::Transform& transform) OVERRIDE;
#endif
  virtual ui::TextureID GetTextureID() OVERRIDE;
  virtual CanvasSkia* AsCanvasSkia() OVERRIDE;
  virtual const CanvasSkia* AsCanvasSkia() const OVERRIDE;
  virtual SkCanvas* GetSkCanvas() OVERRIDE;
  virtual const SkCanvas* GetSkCanvas() const OVERRIDE;

  SkCanvas* sk_canvas() const { return canvas_; }
  skia::PlatformCanvas* platform_canvas() const { return owned_canvas_.get(); }

 private:
  // Test whether the provided rectangle intersects the current clip rect.
  bool IntersectsClipRectInt(int x, int y, int w, int h);

#if defined(OS_WIN)
  // Draws text with the specified color, font and location. The text is
  // aligned to the left, vertically centered, clipped to the region. If the
  // text is too big, it is truncated and '...' is added to the end.
  void DrawStringInt(const string16& text,
                     HFONT font,
                     const SkColor& color,
                     int x, int y, int w, int h,
                     int flags);
#endif

  scoped_ptr<skia::PlatformCanvas> owned_canvas_;
  SkCanvas* canvas_;

  DISALLOW_COPY_AND_ASSIGN(CanvasSkia);
};

}  // namespace gfx

#endif  // UI_GFX_CANVAS_SKIA_H_
