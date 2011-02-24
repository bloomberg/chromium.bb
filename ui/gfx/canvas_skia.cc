// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/canvas_skia.h"

#include <limits>

#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/gfx/brush.h"
#include "ui/gfx/font.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/transform_skia.h"

#if defined(OS_WIN)
#include "ui/gfx/canvas_skia_paint.h"
#endif

namespace {

// A platform wrapper for a Skia shader that makes sure the underlying
// SkShader object is unref'ed when this object is destroyed.
class SkiaShader : public gfx::Brush {
 public:
  explicit SkiaShader(SkShader* shader) : shader_(shader) {
  }
  virtual ~SkiaShader() {
    shader_->unref();
  }

  // Accessor for shader, so it can be associated with a SkPaint.
  SkShader* shader() const { return shader_; }

 private:
  SkShader* shader_;

  DISALLOW_COPY_AND_ASSIGN(SkiaShader);
};


}  // namespace

namespace gfx {

////////////////////////////////////////////////////////////////////////////////
// CanvasSkia, public:

// static
int CanvasSkia::DefaultCanvasTextAlignment() {
  if (!base::i18n::IsRTL())
    return gfx::Canvas::TEXT_ALIGN_LEFT;
  return gfx::Canvas::TEXT_ALIGN_RIGHT;
}

SkBitmap CanvasSkia::ExtractBitmap() const {
  const SkBitmap& device_bitmap = getDevice()->accessBitmap(false);

  // Make a bitmap to return, and a canvas to draw into it. We don't just want
  // to call extractSubset or the copy constructor, since we want an actual copy
  // of the bitmap.
  SkBitmap result;
  device_bitmap.copyTo(&result, SkBitmap::kARGB_8888_Config);
  return result;
}

////////////////////////////////////////////////////////////////////////////////
// CanvasSkia, Canvas implementation:

void CanvasSkia::Save() {
  save();
}

void CanvasSkia::SaveLayerAlpha(uint8 alpha) {
  saveLayerAlpha(NULL, alpha);
}


void CanvasSkia::SaveLayerAlpha(uint8 alpha, const gfx::Rect& layer_bounds) {
  SkRect bounds;
  bounds.set(SkIntToScalar(layer_bounds.x()),
             SkIntToScalar(layer_bounds.y()),
             SkIntToScalar(layer_bounds.right()),
             SkIntToScalar(layer_bounds.bottom()));
  saveLayerAlpha(&bounds, alpha);
}

void CanvasSkia::Restore() {
  restore();
}

bool CanvasSkia::ClipRectInt(int x, int y, int w, int h) {
  SkRect new_clip;
  new_clip.set(SkIntToScalar(x), SkIntToScalar(y),
               SkIntToScalar(x + w), SkIntToScalar(y + h));
  return clipRect(new_clip);
}

void CanvasSkia::TranslateInt(int x, int y) {
  translate(SkIntToScalar(x), SkIntToScalar(y));
}

void CanvasSkia::ScaleInt(int x, int y) {
  scale(SkIntToScalar(x), SkIntToScalar(y));
}

void CanvasSkia::FillRectInt(const SkColor& color, int x, int y, int w, int h) {
  FillRectInt(color, x, y, w, h, SkXfermode::kSrcOver_Mode);
}

void CanvasSkia::FillRectInt(const SkColor& color,
                             int x, int y, int w, int h,
                             SkXfermode::Mode mode) {
  SkPaint paint;
  paint.setColor(color);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setXfermodeMode(mode);
  DrawRectInt(x, y, w, h, paint);
}

void CanvasSkia::FillRectInt(const gfx::Brush* brush,
                             int x, int y, int w, int h) {
  const SkiaShader* shader = static_cast<const SkiaShader*>(brush);
  SkPaint paint;
  paint.setShader(shader->shader());
  // TODO(beng): set shader transform to match canvas transform.
  DrawRectInt(x, y, w, h, paint);
}

void CanvasSkia::DrawRectInt(const SkColor& color, int x, int y, int w, int h) {
  DrawRectInt(color, x, y, w, h, SkXfermode::kSrcOver_Mode);
}

void CanvasSkia::DrawRectInt(const SkColor& color,
                             int x, int y, int w, int h,
                             SkXfermode::Mode mode) {
  SkPaint paint;
  paint.setColor(color);
  paint.setStyle(SkPaint::kStroke_Style);
  // Set a stroke width of 0, which will put us down the stroke rect path.  If
  // we set a stroke width of 1, for example, this will internally create a
  // path and fill it, which causes problems near the edge of the canvas.
  paint.setStrokeWidth(SkIntToScalar(0));
  paint.setXfermodeMode(mode);

  DrawRectInt(x, y, w, h, paint);
}

void CanvasSkia::DrawRectInt(int x, int y, int w, int h, const SkPaint& paint) {
  SkIRect rc = { x, y, x + w, y + h };
  drawIRect(rc, paint);
}

void CanvasSkia::DrawLineInt(const SkColor& color,
                             int x1, int y1,
                             int x2, int y2) {
  SkPaint paint;
  paint.setColor(color);
  paint.setStrokeWidth(SkIntToScalar(1));
  drawLine(SkIntToScalar(x1), SkIntToScalar(y1), SkIntToScalar(x2),
           SkIntToScalar(y2), paint);
}

void CanvasSkia::DrawFocusRect(int x, int y, int width, int height) {
  // Create a 2D bitmap containing alternating on/off pixels - we do this
  // so that you never get two pixels of the same color around the edges
  // of the focus rect (this may mean that opposing edges of the rect may
  // have a dot pattern out of phase to each other).
  static SkBitmap* dots = NULL;
  if (!dots) {
    int col_pixels = 32;
    int row_pixels = 32;

    dots = new SkBitmap;
    dots->setConfig(SkBitmap::kARGB_8888_Config, col_pixels, row_pixels);
    dots->allocPixels();
    dots->eraseARGB(0, 0, 0, 0);

    uint32_t* dot = dots->getAddr32(0, 0);
    for (int i = 0; i < row_pixels; i++) {
      for (int u = 0; u < col_pixels; u++) {
        if ((u % 2 + i % 2) % 2 != 0) {
          dot[i * row_pixels + u] = SK_ColorGRAY;
        }
      }
    }
  }

  // First the horizontal lines.

  // Make a shader for the bitmap with an origin of the box we'll draw. This
  // shader is refcounted and will have an initial refcount of 1.
  SkShader* shader = SkShader::CreateBitmapShader(
      *dots, SkShader::kRepeat_TileMode, SkShader::kRepeat_TileMode);
  // Assign the shader to the paint & release our reference. The paint will
  // now own the shader and the shader will be destroyed when the paint goes
  // out of scope.
  SkPaint paint;
  paint.setShader(shader);
  shader->unref();

  DrawRectInt(x, y, width, 1, paint);
  DrawRectInt(x, y + height - 1, width, 1, paint);
  DrawRectInt(x, y, 1, height, paint);
  DrawRectInt(x + width - 1, y, 1, height, paint);
}

void CanvasSkia::DrawBitmapInt(const SkBitmap& bitmap, int x, int y) {
  drawBitmap(bitmap, SkIntToScalar(x), SkIntToScalar(y));
}

void CanvasSkia::DrawBitmapInt(const SkBitmap& bitmap,
                               int x, int y,
                               const SkPaint& paint) {
  drawBitmap(bitmap, SkIntToScalar(x), SkIntToScalar(y), &paint);
}

void CanvasSkia::DrawBitmapInt(const SkBitmap& bitmap,
                               int src_x, int src_y, int src_w, int src_h,
                               int dest_x, int dest_y, int dest_w, int dest_h,
                               bool filter) {
  SkPaint p;
  DrawBitmapInt(bitmap, src_x, src_y, src_w, src_h, dest_x, dest_y,
                dest_w, dest_h, filter, p);
}

void CanvasSkia::DrawBitmapInt(const SkBitmap& bitmap,
                               int src_x, int src_y, int src_w, int src_h,
                               int dest_x, int dest_y, int dest_w, int dest_h,
                               bool filter,
                               const SkPaint& paint) {
  DLOG_ASSERT(src_x + src_w < std::numeric_limits<int16_t>::max() &&
              src_y + src_h < std::numeric_limits<int16_t>::max());
  if (src_w <= 0 || src_h <= 0 || dest_w <= 0 || dest_h <= 0) {
    NOTREACHED() << "Attempting to draw bitmap to/from an empty rect!";
    return;
  }

  if (!IntersectsClipRectInt(dest_x, dest_y, dest_w, dest_h))
    return;

  SkRect dest_rect = { SkIntToScalar(dest_x),
                       SkIntToScalar(dest_y),
                       SkIntToScalar(dest_x + dest_w),
                       SkIntToScalar(dest_y + dest_h) };

  if (src_w == dest_w && src_h == dest_h) {
    // Workaround for apparent bug in Skia that causes image to occasionally
    // shift.
    SkIRect src_rect = { src_x, src_y, src_x + src_w, src_y + src_h };
    drawBitmapRect(bitmap, &src_rect, dest_rect, &paint);
    return;
  }

  // Make a bitmap shader that contains the bitmap we want to draw. This is
  // basically what SkCanvas.drawBitmap does internally, but it gives us
  // more control over quality and will use the mipmap in the source image if
  // it has one, whereas drawBitmap won't.
  SkShader* shader = SkShader::CreateBitmapShader(bitmap,
                                                  SkShader::kRepeat_TileMode,
                                                  SkShader::kRepeat_TileMode);
  SkMatrix shader_scale;
  shader_scale.setScale(SkFloatToScalar(static_cast<float>(dest_w) / src_w),
                        SkFloatToScalar(static_cast<float>(dest_h) / src_h));
  shader_scale.preTranslate(SkIntToScalar(-src_x), SkIntToScalar(-src_y));
  shader_scale.postTranslate(SkIntToScalar(dest_x), SkIntToScalar(dest_y));
  shader->setLocalMatrix(shader_scale);

  // Set up our paint to use the shader & release our reference (now just owned
  // by the paint).
  SkPaint p(paint);
  p.setFilterBitmap(filter);
  p.setShader(shader);
  shader->unref();

  // The rect will be filled by the bitmap.
  drawRect(dest_rect, p);
}

void CanvasSkia::DrawStringInt(const string16& text,
                               const gfx::Font& font,
                               const SkColor& color,
                               int x, int y, int w, int h) {
  DrawStringInt(text, font, color, x, y, w, h,
                gfx::CanvasSkia::DefaultCanvasTextAlignment());
}

void CanvasSkia::DrawStringInt(const string16& text,
                               const gfx::Font& font,
                               const SkColor& color,
                               const gfx::Rect& display_rect) {
  DrawStringInt(text, font, color, display_rect.x(), display_rect.y(),
                display_rect.width(), display_rect.height());
}

void CanvasSkia::TileImageInt(const SkBitmap& bitmap,
                              int x, int y, int w, int h) {
  TileImageInt(bitmap, 0, 0, x, y, w, h);
}

void CanvasSkia::TileImageInt(const SkBitmap& bitmap,
                              int src_x, int src_y,
                              int dest_x, int dest_y, int w, int h) {
  if (!IntersectsClipRectInt(dest_x, dest_y, w, h))
    return;

  SkPaint paint;

  SkShader* shader = SkShader::CreateBitmapShader(bitmap,
                                                  SkShader::kRepeat_TileMode,
                                                  SkShader::kRepeat_TileMode);
  paint.setShader(shader);
  paint.setXfermodeMode(SkXfermode::kSrcOver_Mode);

  // CreateBitmapShader returns a Shader with a reference count of one, we
  // need to unref after paint takes ownership of the shader.
  shader->unref();
  save();
  translate(SkIntToScalar(dest_x - src_x), SkIntToScalar(dest_y - src_y));
  ClipRectInt(src_x, src_y, w, h);
  drawPaint(paint);
  restore();
}

gfx::NativeDrawingContext CanvasSkia::BeginPlatformPaint() {
  return beginPlatformPaint();
}

void CanvasSkia::EndPlatformPaint() {
  endPlatformPaint();
}

void CanvasSkia::Transform(const ui::Transform& transform) {
  concat(*reinterpret_cast<const ui::TransformSkia&>(transform).matrix_.get());
}

CanvasSkia* CanvasSkia::AsCanvasSkia() {
  return this;
}

const CanvasSkia* CanvasSkia::AsCanvasSkia() const {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// CanvasSkia, private:

bool CanvasSkia::IntersectsClipRectInt(int x, int y, int w, int h) {
  SkRect clip;
  return getClipBounds(&clip) &&
      clip.intersect(SkIntToScalar(x), SkIntToScalar(y), SkIntToScalar(x + w),
                     SkIntToScalar(y + h));
}

////////////////////////////////////////////////////////////////////////////////
// Canvas, public:

Canvas* Canvas::CreateCanvas() {
  return new CanvasSkia;
}

Canvas* Canvas::CreateCanvas(int width, int height, bool is_opaque) {
  return new CanvasSkia(width, height, is_opaque);
}

#if defined(OS_WIN)
// TODO(beng): move to canvas_win.cc, etc.
class CanvasPaintWin : public CanvasSkiaPaint, public CanvasPaint {
 public:
  CanvasPaintWin(gfx::NativeView view) : CanvasSkiaPaint(view) {}

  // Overridden from CanvasPaint2:
  virtual bool IsValid() const {
    return isEmpty();
  }

  virtual gfx::Rect GetInvalidRect() const {
    return gfx::Rect(paintStruct().rcPaint);
  }

  virtual Canvas* AsCanvas() {
    return this;
  }
};
#endif

CanvasPaint* CanvasPaint::CreateCanvasPaint(gfx::NativeView view) {
#if defined(OS_WIN)
  return new CanvasPaintWin(view);
#else
  return NULL;
#endif
}

}  // namespace gfx
