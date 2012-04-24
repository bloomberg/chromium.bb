// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/canvas.h"

#include <limits>

#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/transform.h"

#if defined(OS_WIN)
#include "ui/gfx/canvas_skia_paint.h"
#endif

namespace gfx {

Canvas::Canvas(const gfx::Size& size, bool is_opaque)
    : owned_canvas_(new skia::PlatformCanvas(size.width(), size.height(),
                                             is_opaque)),
      canvas_(owned_canvas_.get()) {
#if defined(OS_WIN) || defined(OS_MACOSX)
  // skia::PlatformCanvas instances are initialized to 0 by Cairo on Linux, but
  // uninitialized on Win and Mac.
  if (!is_opaque)
    owned_canvas_->clear(SkColorSetARGB(0, 0, 0, 0));
#endif
}

Canvas::Canvas(const SkBitmap& bitmap, bool is_opaque)
    : owned_canvas_(new skia::PlatformCanvas(bitmap.width(), bitmap.height(),
                                             is_opaque)),
      canvas_(owned_canvas_.get()) {
  DrawBitmapInt(bitmap, 0, 0);
}

Canvas::Canvas()
    : owned_canvas_(new skia::PlatformCanvas()),
      canvas_(owned_canvas_.get()) {
}

Canvas::Canvas(SkCanvas* canvas)
    : owned_canvas_(),
      canvas_(canvas) {
  DCHECK(canvas);
}

Canvas::~Canvas() {
}

// static
int Canvas::GetStringWidth(const string16& text, const gfx::Font& font) {
  int width = 0, height = 0;
  Canvas::SizeStringInt(text, font, &width, &height, NO_ELLIPSIS);
  return width;
}

// static
int Canvas::DefaultCanvasTextAlignment() {
  return base::i18n::IsRTL() ? TEXT_ALIGN_RIGHT : TEXT_ALIGN_LEFT;
}

SkBitmap Canvas::ExtractBitmap() const {
  const SkBitmap& device_bitmap = canvas_->getDevice()->accessBitmap(false);

  // Make a bitmap to return, and a canvas to draw into it. We don't just want
  // to call extractSubset or the copy constructor, since we want an actual copy
  // of the bitmap.
  SkBitmap result;
  device_bitmap.copyTo(&result, SkBitmap::kARGB_8888_Config);
  return result;
}

void Canvas::DrawDashedRect(const gfx::Rect& rect, SkColor color) {
  // Create a 2D bitmap containing alternating on/off pixels - we do this
  // so that you never get two pixels of the same color around the edges
  // of the focus rect (this may mean that opposing edges of the rect may
  // have a dot pattern out of phase to each other).
  static SkColor last_color;
  static SkBitmap* dots = NULL;
  if (!dots || last_color != color) {
    int col_pixels = 32;
    int row_pixels = 32;

    delete dots;
    last_color = color;
    dots = new SkBitmap;
    dots->setConfig(SkBitmap::kARGB_8888_Config, col_pixels, row_pixels);
    dots->allocPixels();
    dots->eraseARGB(0, 0, 0, 0);

    uint32_t* dot = dots->getAddr32(0, 0);
    for (int i = 0; i < row_pixels; i++) {
      for (int u = 0; u < col_pixels; u++) {
        if ((u % 2 + i % 2) % 2 != 0) {
          dot[i * row_pixels + u] = color;
        }
      }
    }
  }

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

  DrawRect(gfx::Rect(rect.x(), rect.y(), rect.width(), 1), paint);
  DrawRect(gfx::Rect(rect.x(), rect.y() + rect.height() - 1, rect.width(), 1),
           paint);
  DrawRect(gfx::Rect(rect.x(), rect.y(), 1, rect.height()), paint);
  DrawRect(gfx::Rect(rect.x() + rect.width() - 1, rect.y(), 1, rect.height()),
           paint);
}

void Canvas::Save() {
  canvas_->save();
}

void Canvas::SaveLayerAlpha(uint8 alpha) {
  canvas_->saveLayerAlpha(NULL, alpha);
}


void Canvas::SaveLayerAlpha(uint8 alpha, const gfx::Rect& layer_bounds) {
  SkRect bounds(gfx::RectToSkRect(layer_bounds));
  canvas_->saveLayerAlpha(&bounds, alpha);
}

void Canvas::Restore() {
  canvas_->restore();
}

bool Canvas::ClipRect(const gfx::Rect& rect) {
  return canvas_->clipRect(gfx::RectToSkRect(rect));
}

bool Canvas::ClipPath(const SkPath& path) {
  return canvas_->clipPath(path);
}

bool Canvas::GetClipBounds(gfx::Rect* bounds) {
  SkRect out;
  bool has_non_empty_clip = canvas_->getClipBounds(&out);
  bounds->SetRect(out.left(), out.top(), out.width(), out.height());
  return has_non_empty_clip;
}

void Canvas::Translate(const gfx::Point& point) {
  canvas_->translate(SkIntToScalar(point.x()), SkIntToScalar(point.y()));
}

void Canvas::Scale(int x_scale, int y_scale) {
  canvas_->scale(SkIntToScalar(x_scale), SkIntToScalar(y_scale));
}

void Canvas::DrawColor(SkColor color) {
  DrawColor(color, SkXfermode::kSrcOver_Mode);
}

void Canvas::DrawColor(SkColor color, SkXfermode::Mode mode) {
  canvas_->drawColor(color, mode);
}

void Canvas::FillRect(const gfx::Rect& rect, SkColor color) {
  FillRect(rect, color, SkXfermode::kSrcOver_Mode);
}

void Canvas::FillRect(const gfx::Rect& rect,
                      SkColor color,
                      SkXfermode::Mode mode) {
  SkPaint paint;
  paint.setColor(color);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setXfermodeMode(mode);
  DrawRect(rect, paint);
}

void Canvas::DrawRect(const gfx::Rect& rect, SkColor color) {
  DrawRect(rect, color, SkXfermode::kSrcOver_Mode);
}

void Canvas::DrawRect(const gfx::Rect& rect,
                      SkColor color,
                      SkXfermode::Mode mode) {
  SkPaint paint;
  paint.setColor(color);
  paint.setStyle(SkPaint::kStroke_Style);
  // Set a stroke width of 0, which will put us down the stroke rect path.  If
  // we set a stroke width of 1, for example, this will internally create a
  // path and fill it, which causes problems near the edge of the canvas.
  paint.setStrokeWidth(SkIntToScalar(0));
  paint.setXfermodeMode(mode);

  DrawRect(rect, paint);
}

void Canvas::DrawRect(const gfx::Rect& rect, const SkPaint& paint) {
  canvas_->drawIRect(RectToSkIRect(rect), paint);
}

void Canvas::DrawPoint(const gfx::Point& p1, const SkPaint& paint) {
  canvas_->drawPoint(SkIntToScalar(p1.x()), SkIntToScalar(p1.y()), paint);
}

void Canvas::DrawLine(const gfx::Point& p1,
                      const gfx::Point& p2,
                      SkColor color) {
  SkPaint paint;
  paint.setColor(color);
  paint.setStrokeWidth(SkIntToScalar(1));
  DrawLine(p1, p2, paint);
}

void Canvas::DrawLine(const gfx::Point& p1,
                      const gfx::Point& p2,
                      const SkPaint& paint) {
  canvas_->drawLine(SkIntToScalar(p1.x()), SkIntToScalar(p1.y()),
                    SkIntToScalar(p2.x()), SkIntToScalar(p2.y()), paint);
}

void Canvas::DrawCircle(const gfx::Point& center_point,
                        int radius,
                        const SkPaint& paint) {
  canvas_->drawCircle(SkIntToScalar(center_point.x()),
      SkIntToScalar(center_point.y()), SkIntToScalar(radius), paint);
}

void Canvas::DrawRoundRect(const gfx::Rect& rect,
                           int radius,
                           const SkPaint& paint) {
  canvas_->drawRoundRect(RectToSkRect(rect), SkIntToScalar(radius),
                         SkIntToScalar(radius), paint);
}

void Canvas::DrawPath(const SkPath& path, const SkPaint& paint) {
  canvas_->drawPath(path, paint);
}

void Canvas::DrawFocusRect(const gfx::Rect& rect) {
  DrawDashedRect(rect, SK_ColorGRAY);
}

void Canvas::DrawBitmapInt(const SkBitmap& bitmap, int x, int y) {
  canvas_->drawBitmap(bitmap, SkIntToScalar(x), SkIntToScalar(y));
}

void Canvas::DrawBitmapInt(const SkBitmap& bitmap,
                           int x, int y,
                           const SkPaint& paint) {
  canvas_->drawBitmap(bitmap, SkIntToScalar(x), SkIntToScalar(y), &paint);
}

void Canvas::DrawBitmapInt(const SkBitmap& bitmap,
                           int src_x, int src_y, int src_w, int src_h,
                           int dest_x, int dest_y, int dest_w, int dest_h,
                           bool filter) {
  SkPaint p;
  DrawBitmapInt(bitmap, src_x, src_y, src_w, src_h, dest_x, dest_y,
                dest_w, dest_h, filter, p);
}

void Canvas::DrawBitmapInt(const SkBitmap& bitmap,
                           int src_x, int src_y, int src_w, int src_h,
                           int dest_x, int dest_y, int dest_w, int dest_h,
                           bool filter,
                           const SkPaint& paint) {
  DrawBitmapFloat(bitmap, static_cast<float>(src_x), static_cast<float>(src_y),
                  static_cast<float>(src_w), static_cast<float>(src_h),
                  static_cast<float>(dest_x), static_cast<float>(dest_y),
                  static_cast<float>(dest_w), static_cast<float>(dest_h),
                  filter, paint);
}

void Canvas::DrawBitmapFloat(const SkBitmap& bitmap,
    float src_x, float src_y, float src_w, float src_h,
    float dest_x, float dest_y, float dest_w, float dest_h,
    bool filter,
    const SkPaint& paint) {
  DLOG_ASSERT(src_x + src_w < std::numeric_limits<int16_t>::max() &&
              src_y + src_h < std::numeric_limits<int16_t>::max());
  if (src_w <= 0 || src_h <= 0) {
    NOTREACHED() << "Attempting to draw bitmap from an empty rect!";
    return;
  }

  if (!IntersectsClipRectInt(dest_x, dest_y, dest_w, dest_h))
    return;

  SkRect dest_rect = { SkFloatToScalar(dest_x),
                       SkFloatToScalar(dest_y),
                       SkFloatToScalar(dest_x + dest_w),
                       SkFloatToScalar(dest_y + dest_h) };

  if (src_w == dest_w && src_h == dest_h) {
    // Workaround for apparent bug in Skia that causes image to occasionally
    // shift.
    SkIRect src_rect = { src_x, src_y, src_x + src_w, src_y + src_h };
    canvas_->drawBitmapRect(bitmap, &src_rect, dest_rect, &paint);
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
  shader_scale.setScale(SkFloatToScalar(dest_w / src_w),
                        SkFloatToScalar(dest_h / src_h));
  shader_scale.preTranslate(SkFloatToScalar(-src_x), SkFloatToScalar(-src_y));
  shader_scale.postTranslate(SkFloatToScalar(dest_x), SkFloatToScalar(dest_y));
  shader->setLocalMatrix(shader_scale);

  // Set up our paint to use the shader & release our reference (now just owned
  // by the paint).
  SkPaint p(paint);
  p.setFilterBitmap(filter);
  p.setShader(shader);
  shader->unref();

  // The rect will be filled by the bitmap.
  canvas_->drawRect(dest_rect, p);
}

void Canvas::DrawStringInt(const string16& text,
                           const gfx::Font& font,
                           SkColor color,
                           int x, int y, int w, int h) {
  DrawStringInt(text, font, color, x, y, w, h, DefaultCanvasTextAlignment());
}

void Canvas::DrawStringInt(const string16& text,
                           const gfx::Font& font,
                           SkColor color,
                           const gfx::Rect& display_rect) {
  DrawStringInt(text, font, color, display_rect.x(), display_rect.y(),
                display_rect.width(), display_rect.height());
}

void Canvas::DrawStringInt(const string16& text,
                           const gfx::Font& font,
                           SkColor color,
                           int x, int y, int w, int h,
                           int flags) {
  DrawStringWithShadows(text,
                        font,
                        color,
                        gfx::Rect(x, y, w, h),
                        flags,
                        std::vector<ShadowValue>());
}

void Canvas::TileImageInt(const SkBitmap& bitmap,
                          int x, int y, int w, int h) {
  TileImageInt(bitmap, 0, 0, x, y, w, h);
}

void Canvas::TileImageInt(const SkBitmap& bitmap,
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
  canvas_->save();
  canvas_->translate(SkIntToScalar(dest_x - src_x),
                     SkIntToScalar(dest_y - src_y));
  ClipRect(gfx::Rect(src_x, src_y, w, h));
  canvas_->drawPaint(paint);
  canvas_->restore();
}

gfx::NativeDrawingContext Canvas::BeginPlatformPaint() {
  return skia::BeginPlatformPaint(canvas_);
}

void Canvas::EndPlatformPaint() {
  skia::EndPlatformPaint(canvas_);
}

void Canvas::Transform(const ui::Transform& transform) {
  canvas_->concat(transform.matrix());
}

bool Canvas::IntersectsClipRectInt(int x, int y, int w, int h) {
  SkRect clip;
  return canvas_->getClipBounds(&clip) &&
      clip.intersect(SkIntToScalar(x), SkIntToScalar(y), SkIntToScalar(x + w),
                     SkIntToScalar(y + h));
}

bool Canvas::IntersectsClipRect(const gfx::Rect& rect) {
  return IntersectsClipRectInt(rect.x(), rect.y(),
                               rect.width(), rect.height());
}

}  // namespace gfx
