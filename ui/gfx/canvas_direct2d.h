// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_CANVAS_DIRECT2D_H_
#define UI_GFX_CANVAS_DIRECT2D_H_
#pragma once

#include <d2d1.h>

#include <stack>

#include "base/compiler_specific.h"
#include "base/win/scoped_comptr.h"
#include "ui/gfx/canvas.h"

namespace gfx {

class UI_EXPORT CanvasDirect2D : public Canvas {
 public:
  // Creates an empty Canvas.
  explicit CanvasDirect2D(ID2D1RenderTarget* rt);
  virtual ~CanvasDirect2D();

  // Retrieves the application's D2D1 Factory.
  static ID2D1Factory* GetD2D1Factory();

  // Overridden from Canvas:
  virtual void Save() OVERRIDE;
  virtual void SaveLayerAlpha(uint8 alpha) OVERRIDE;
  virtual void SaveLayerAlpha(uint8 alpha,
                              const gfx::Rect& layer_bounds) OVERRIDE;
  virtual void Restore() OVERRIDE;
  virtual bool ClipRect(const gfx::Rect& rect) OVERRIDE;
  virtual void Translate(const gfx::Point& point) OVERRIDE;
  virtual void ScaleInt(int x, int y) OVERRIDE;
  virtual void FillRectInt(const SkColor& color,
                           int x, int y, int w, int h) OVERRIDE;
  virtual void FillRectInt(const SkColor& color,
                           int x, int y, int w, int h,
                           SkXfermode::Mode mode) OVERRIDE;
  virtual void FillRectInt(const gfx::Brush* brush,
                           int x, int y, int w, int h) OVERRIDE;
  virtual void DrawRectInt(const SkColor& color,
                           int x, int y, int w, int h) OVERRIDE;
  virtual void DrawRectInt(const SkColor& color,
                           int x, int y, int w, int h,
                           SkXfermode::Mode mode) OVERRIDE;
  virtual void DrawRectInt(int x, int y, int w, int h,
                           const SkPaint& paint) OVERRIDE;
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
  virtual void DrawFocusRect(const gfx::Rect& rect) OVERRIDE;
  virtual void TileImageInt(const SkBitmap& bitmap,
                            int x, int y, int w, int h) OVERRIDE;
  virtual void TileImageInt(const SkBitmap& bitmap,
                            int src_x, int src_y,
                            int dest_x, int dest_y, int w, int h) OVERRIDE;
  virtual gfx::NativeDrawingContext BeginPlatformPaint() OVERRIDE;
  virtual void EndPlatformPaint() OVERRIDE;
  virtual void Transform(const ui::Transform& transform) OVERRIDE;
  virtual ui::TextureID GetTextureID() OVERRIDE;
  virtual CanvasSkia* AsCanvasSkia() OVERRIDE;
  virtual const CanvasSkia* AsCanvasSkia() const OVERRIDE;

 private:
  void SaveInternal(ID2D1Layer* layer);

  ID2D1RenderTarget* rt_;
  base::win::ScopedComPtr<ID2D1GdiInteropRenderTarget> interop_rt_;
  base::win::ScopedComPtr<ID2D1DrawingStateBlock> drawing_state_block_;
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

}  // namespace gfx

#endif  // UI_GFX_CANVAS_DIRECT2D_H_
