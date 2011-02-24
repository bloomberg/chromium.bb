// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_CANVAS_DIRECT2D_H_
#define UI_GFX_CANVAS_DIRECT2D_H_
#pragma once

#include <d2d1.h>

#include <stack>

#include "base/scoped_comptr_win.h"
#include "ui/gfx/canvas.h"

namespace gfx {

class CanvasDirect2D : public Canvas {
 public:
  // Creates an empty Canvas.
  explicit CanvasDirect2D(ID2D1RenderTarget* rt);
  virtual ~CanvasDirect2D();

  // Retrieves the application's D2D1 Factory.
  static ID2D1Factory* GetD2D1Factory();

  // Overridden from Canvas:
  virtual void Save();
  virtual void SaveLayerAlpha(uint8 alpha);
  virtual void SaveLayerAlpha(uint8 alpha, const gfx::Rect& layer_bounds);
  virtual void Restore();
  virtual bool ClipRectInt(int x, int y, int w, int h);
  virtual void TranslateInt(int x, int y);
  virtual void ScaleInt(int x, int y);
  virtual void FillRectInt(const SkColor& color, int x, int y, int w, int h);
  virtual void FillRectInt(const SkColor& color, int x, int y, int w, int h,
                           SkXfermode::Mode mode);
  virtual void FillRectInt(const gfx::Brush* brush, int x, int y, int w, int h);
  virtual void DrawRectInt(const SkColor& color, int x, int y, int w, int h);
  virtual void DrawRectInt(const SkColor& color,
                           int x, int y, int w, int h,
                           SkXfermode::Mode mode);
  virtual void DrawRectInt(int x, int y, int w, int h, const SkPaint& paint);
  virtual void DrawLineInt(const SkColor& color,
                           int x1, int y1,
                           int x2, int y2);
  virtual void DrawBitmapInt(const SkBitmap& bitmap, int x, int y);
  virtual void DrawBitmapInt(const SkBitmap& bitmap,
                             int x, int y,
                             const SkPaint& paint);
  virtual void DrawBitmapInt(const SkBitmap& bitmap,
                             int src_x, int src_y, int src_w, int src_h,
                             int dest_x, int dest_y, int dest_w, int dest_h,
                             bool filter);
  virtual void DrawBitmapInt(const SkBitmap& bitmap,
                             int src_x, int src_y, int src_w, int src_h,
                             int dest_x, int dest_y, int dest_w, int dest_h,
                             bool filter,
                             const SkPaint& paint);
  virtual void DrawStringInt(const string16& text,
                             const gfx::Font& font,
                             const SkColor& color,
                             int x, int y, int w, int h);
  virtual void DrawStringInt(const string16& text,
                             const gfx::Font& font,
                             const SkColor& color,
                             const gfx::Rect& display_rect);
  virtual void DrawStringInt(const string16& text,
                             const gfx::Font& font,
                             const SkColor& color,
                             int x, int y, int w, int h,
                             int flags);
  virtual void DrawFocusRect(int x, int y, int width, int height);
  virtual void TileImageInt(const SkBitmap& bitmap, int x, int y, int w, int h);
  virtual void TileImageInt(const SkBitmap& bitmap,
                            int src_x, int src_y,
                            int dest_x, int dest_y, int w, int h);
  virtual gfx::NativeDrawingContext BeginPlatformPaint();
  virtual void EndPlatformPaint();
  virtual void Transform(const ui::Transform& transform);
  virtual CanvasSkia* AsCanvasSkia();
  virtual const CanvasSkia* AsCanvasSkia() const;

 private:
  void SaveInternal(ID2D1Layer* layer);

  ID2D1RenderTarget* rt_;
  ScopedComPtr<ID2D1GdiInteropRenderTarget> interop_rt_;
  ScopedComPtr<ID2D1DrawingStateBlock> drawing_state_block_;
  static ID2D1Factory* d2d1_factory_;

  // Every time Save* is called, a RenderState object is pushed onto the
  // RenderState stack.
  struct RenderState {
    explicit RenderState(ID2D1Layer* layer) : layer(layer), clip_count(0) {}
    RenderState() : layer(NULL), clip_count(0) {}

    // A D2D layer associated with this state, or NULL if there is no layer.
    // The layer is created and owned by the Canvas.
    ID2D1Layer* layer;
    // The number of clip operations performed. This is used to balance calls to
    // PushAxisAlignedClip with calls to PopAxisAlignedClip when Restore() is
    // called.
    int clip_count;
  };
  std::stack<RenderState> state_;

  DISALLOW_COPY_AND_ASSIGN(CanvasDirect2D);
};

}  // namespace gfx;

#endif  // UI_GFX_CANVAS_DIRECT2D_H_
