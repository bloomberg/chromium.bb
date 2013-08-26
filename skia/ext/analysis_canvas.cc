// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "skia/ext/analysis_canvas.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "third_party/skia/include/core/SkDraw.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "third_party/skia/include/core/SkShader.h"
#include "third_party/skia/src/core/SkRasterClip.h"
#include "ui/gfx/rect_conversions.h"

namespace {

const int kNoLayer = -1;

bool IsSolidColorPaint(const SkPaint& paint) {
  SkXfermode::Mode xfermode;

  // getXfermode can return a NULL, but that is handled
  // gracefully by AsMode (NULL turns into kSrcOver mode).
  SkXfermode::AsMode(paint.getXfermode(), &xfermode);

  // Paint is solid color if the following holds:
  // - Alpha is 1.0, style is fill, and there are no special effects
  // - Xfer mode is either kSrc or kSrcOver (kSrcOver is equivalent
  //   to kSrc if source alpha is 1.0, which is already checked).
  return (paint.getAlpha() == 255 &&
          !paint.getShader() &&
          !paint.getLooper() &&
          !paint.getMaskFilter() &&
          !paint.getColorFilter() &&
          paint.getStyle() == SkPaint::kFill_Style &&
          (xfermode == SkXfermode::kSrc_Mode ||
           xfermode == SkXfermode::kSrcOver_Mode));
}

bool IsFullQuad(const SkDraw& draw,
                const SkRect& canvas_rect,
                const SkRect& drawn_rect) {

  // If the transform results in a non-axis aligned
  // rect, then be conservative and return false.
  if (!draw.fMatrix->rectStaysRect())
    return false;

  SkRect draw_bitmap_rect;
  draw.fBitmap->getBounds(&draw_bitmap_rect);
  SkRect clip_rect = SkRect::Make(draw.fRC->getBounds());
  SkRect device_rect;
  draw.fMatrix->mapRect(&device_rect, drawn_rect);

  // The drawn rect covers the full canvas, if the following conditions hold:
  // - Clip rect is an actual rectangle.
  // - The rect we're drawing (post-transform) contains the clip rect.
  //   That is, all of clip rect will be colored by the rect.
  // - Clip rect contains the canvas rect.
  //   That is, we're not clipping to a portion of this canvas.
  // - The bitmap into which the draw call happens is at least as
  //   big as the canvas rect
  return draw.fRC->isRect() &&
         device_rect.contains(clip_rect) &&
         clip_rect.contains(canvas_rect) &&
         draw_bitmap_rect.contains(canvas_rect);
}

} // namespace

namespace skia {

AnalysisDevice::AnalysisDevice(const SkBitmap& bitmap)
    : INHERITED(bitmap),
      is_forced_not_solid_(false),
      is_forced_not_transparent_(false),
      is_solid_color_(true),
      is_transparent_(true),
      has_text_(false) {}

AnalysisDevice::~AnalysisDevice() {}

bool AnalysisDevice::GetColorIfSolid(SkColor* color) const {
  if (is_transparent_) {
    *color = SK_ColorTRANSPARENT;
    return true;
  }
  if (is_solid_color_) {
    *color = color_;
    return true;
  }
  return false;
}

bool AnalysisDevice::HasText() const {
  return has_text_;
}

void AnalysisDevice::SetForceNotSolid(bool flag) {
  is_forced_not_solid_ = flag;
  if (is_forced_not_solid_)
    is_solid_color_ = false;
}

void AnalysisDevice::SetForceNotTransparent(bool flag) {
  is_forced_not_transparent_ = flag;
  if (is_forced_not_transparent_)
    is_transparent_ = false;
}

void AnalysisDevice::clear(SkColor color) {
  is_transparent_ = (!is_forced_not_transparent_ && SkColorGetA(color) == 0);
  has_text_ = false;

  if (!is_forced_not_solid_ && SkColorGetA(color) == 255) {
    is_solid_color_ = true;
    color_ = color;
  } else {
    is_solid_color_ = false;
  }
}

void AnalysisDevice::drawPaint(const SkDraw& draw, const SkPaint& paint) {
  is_solid_color_ = false;
  is_transparent_ = false;
}

void AnalysisDevice::drawPoints(const SkDraw& draw,
                                SkCanvas::PointMode mode,
                                size_t count,
                                const SkPoint points[],
                                const SkPaint& paint) {
  is_solid_color_ = false;
  is_transparent_ = false;
}

void AnalysisDevice::drawRect(const SkDraw& draw,
                              const SkRect& rect,
                              const SkPaint& paint) {
  bool does_cover_canvas =
      IsFullQuad(draw, SkRect::MakeWH(width(), height()), rect);

  SkXfermode::Mode xfermode;
  SkXfermode::AsMode(paint.getXfermode(), &xfermode);

  // This canvas will become transparent if the following holds:
  // - The quad is a full tile quad
  // - We're not in "forced not transparent" mode
  // - Transfer mode is clear (0 color, 0 alpha)
  //
  // If the paint alpha is not 0, or if the transfrer mode is
  // not src, then this canvas will not be transparent.
  //
  // In all other cases, we keep the current transparent value
  if (does_cover_canvas &&
      !is_forced_not_transparent_ &&
      xfermode == SkXfermode::kClear_Mode) {
    is_transparent_ = true;
    has_text_ = false;
  } else if (paint.getAlpha() != 0 || xfermode != SkXfermode::kSrc_Mode) {
    is_transparent_ = false;
  }

  // This bitmap is solid if and only if the following holds.
  // Note that this might be overly conservative:
  // - We're not in "forced not solid" mode
  // - Paint is solid color
  // - The quad is a full tile quad
  if (!is_forced_not_solid_ && IsSolidColorPaint(paint) && does_cover_canvas) {
    is_solid_color_ = true;
    color_ = paint.getColor();
    has_text_ = false;
  } else {
    is_solid_color_ = false;
  }
}

void AnalysisDevice::drawOval(const SkDraw& draw,
                              const SkRect& oval,
                              const SkPaint& paint) {
  is_solid_color_ = false;
  is_transparent_ = false;
}

void AnalysisDevice::drawPath(const SkDraw& draw,
                              const SkPath& path,
                              const SkPaint& paint,
                              const SkMatrix* pre_path_matrix,
                              bool path_is_mutable) {
  is_solid_color_ = false;
  is_transparent_ = false;
}

void AnalysisDevice::drawBitmap(const SkDraw& draw,
                                const SkBitmap& bitmap,
                                const SkMatrix& matrix,
                                const SkPaint& paint) {
  is_solid_color_ = false;
  is_transparent_ = false;
}

void AnalysisDevice::drawSprite(const SkDraw& draw,
                                const SkBitmap& bitmap,
                                int x,
                                int y,
                                const SkPaint& paint) {
  is_solid_color_ = false;
  is_transparent_ = false;
}

void AnalysisDevice::drawBitmapRect(const SkDraw& draw,
                                    const SkBitmap& bitmap,
                                    const SkRect* src_or_null,
                                    const SkRect& dst,
                                    const SkPaint& paint,
                                    SkCanvas::DrawBitmapRectFlags flags) {
  // Call drawRect to determine transparency,
  // but reset solid color to false.
  drawRect(draw, dst, paint);
  is_solid_color_ = false;
}

void AnalysisDevice::drawText(const SkDraw& draw,
                              const void* text,
                              size_t len,
                              SkScalar x,
                              SkScalar y,
                              const SkPaint& paint) {
  is_solid_color_ = false;
  is_transparent_ = false;
  has_text_ = true;
}

void AnalysisDevice::drawPosText(const SkDraw& draw,
                                 const void* text,
                                 size_t len,
                                 const SkScalar pos[],
                                 SkScalar const_y,
                                 int scalars_per_pos,
                                 const SkPaint& paint) {
  is_solid_color_ = false;
  is_transparent_ = false;
  has_text_ = true;
}

void AnalysisDevice::drawTextOnPath(const SkDraw& draw,
                                    const void* text,
                                    size_t len,
                                    const SkPath& path,
                                    const SkMatrix* matrix,
                                    const SkPaint& paint) {
  is_solid_color_ = false;
  is_transparent_ = false;
  has_text_ = true;
}

#ifdef SK_BUILD_FOR_ANDROID
void AnalysisDevice::drawPosTextOnPath(const SkDraw& draw,
                                       const void* text,
                                       size_t len,
                                       const SkPoint pos[],
                                       const SkPaint& paint,
                                       const SkPath& path,
                                       const SkMatrix* matrix) {
  is_solid_color_ = false;
  is_transparent_ = false;
  has_text_ = true;
}
#endif

void AnalysisDevice::drawVertices(const SkDraw& draw,
                                  SkCanvas::VertexMode,
                                  int vertex_count,
                                  const SkPoint verts[],
                                  const SkPoint texs[],
                                  const SkColor colors[],
                                  SkXfermode* xmode,
                                  const uint16_t indices[],
                                  int index_count,
                                  const SkPaint& paint) {
  is_solid_color_ = false;
  is_transparent_ = false;
}

void AnalysisDevice::drawDevice(const SkDraw& draw,
                                SkBaseDevice* device,
                                int x,
                                int y,
                                const SkPaint& paint) {
  is_solid_color_ = false;
  is_transparent_ = false;
}

AnalysisCanvas::AnalysisCanvas(AnalysisDevice* device)
    : INHERITED(device),
      saved_stack_size_(0),
      force_not_solid_stack_level_(kNoLayer),
      force_not_transparent_stack_level_(kNoLayer) {}

AnalysisCanvas::~AnalysisCanvas() {}

bool AnalysisCanvas::GetColorIfSolid(SkColor* color) const {
  return (static_cast<AnalysisDevice*>(getDevice()))->GetColorIfSolid(color);
}

bool AnalysisCanvas::HasText() const {
  return (static_cast<AnalysisDevice*>(getDevice()))->HasText();
}

bool AnalysisCanvas::abortDrawing() {
  // Early out as soon as we have detected that the tile has text.
  return HasText();
}

bool AnalysisCanvas::clipRect(const SkRect& rect, SkRegion::Op op, bool do_aa) {
  return INHERITED::clipRect(rect, op, do_aa);
}

bool AnalysisCanvas::clipPath(const SkPath& path, SkRegion::Op op, bool do_aa) {
  // clipPaths can make our calls to IsFullQuad invalid (ie have false
  // positives). As a precaution, force the setting to be non-solid
  // and non-transparent until we pop this
  if (force_not_solid_stack_level_ == kNoLayer) {
    force_not_solid_stack_level_ = saved_stack_size_;
    (static_cast<AnalysisDevice*>(getDevice()))->SetForceNotSolid(true);
  }
  if (force_not_transparent_stack_level_ == kNoLayer) {
    force_not_transparent_stack_level_ = saved_stack_size_;
    (static_cast<AnalysisDevice*>(getDevice()))->SetForceNotTransparent(true);
  }

  return INHERITED::clipRect(path.getBounds(), op, do_aa);
}

bool AnalysisCanvas::clipRRect(const SkRRect& rrect,
                               SkRegion::Op op,
                               bool do_aa) {
  // clipRRect can make our calls to IsFullQuad invalid (ie have false
  // positives). As a precaution, force the setting to be non-solid
  // and non-transparent until we pop this
  if (force_not_solid_stack_level_ == kNoLayer) {
    force_not_solid_stack_level_ = saved_stack_size_;
    (static_cast<AnalysisDevice*>(getDevice()))->SetForceNotSolid(true);
  }
  if (force_not_transparent_stack_level_ == kNoLayer) {
    force_not_transparent_stack_level_ = saved_stack_size_;
    (static_cast<AnalysisDevice*>(getDevice()))->SetForceNotTransparent(true);
  }

  return INHERITED::clipRect(rrect.getBounds(), op, do_aa);
}

int AnalysisCanvas::save(SkCanvas::SaveFlags flags) {
  ++saved_stack_size_;
  return INHERITED::save(flags);
}

int AnalysisCanvas::saveLayer(const SkRect* bounds,
                              const SkPaint* paint,
                              SkCanvas::SaveFlags flags) {
  ++saved_stack_size_;

  // If after we draw to the saved layer, we have to blend with the current
  // layer, then we can conservatively say that the canvas will not be of
  // solid color.
  if ((paint && !IsSolidColorPaint(*paint)) ||
      (bounds && !bounds->contains(SkRect::MakeWH(getDevice()->width(),
                                                  getDevice()->height())))) {
    if (force_not_solid_stack_level_ == kNoLayer) {
      force_not_solid_stack_level_ = saved_stack_size_;
      (static_cast<AnalysisDevice*>(getDevice()))->SetForceNotSolid(true);
    }
  }

  // If after we draw to the save layer, we have to blend with the current
  // layer using any part of the current layer's alpha, then we can
  // conservatively say that the canvas will not be transparent.
  SkXfermode::Mode xfermode = SkXfermode::kSrc_Mode;
  if (paint)
    SkXfermode::AsMode(paint->getXfermode(), &xfermode);
  if (xfermode != SkXfermode::kSrc_Mode) {
    if (force_not_transparent_stack_level_ == kNoLayer) {
      force_not_transparent_stack_level_ = saved_stack_size_;
      (static_cast<AnalysisDevice*>(getDevice()))->SetForceNotTransparent(true);
    }
  }

  // Actually saving a layer here could cause a new bitmap to be created
  // and real rendering to occur.
  int count = INHERITED::save(flags);
  if (bounds) {
    INHERITED::clipRectBounds(bounds, flags, NULL);
  }
  return count;
}

void AnalysisCanvas::restore() {
  INHERITED::restore();

  DCHECK(saved_stack_size_);
  if (saved_stack_size_) {
    --saved_stack_size_;
    if (saved_stack_size_ < force_not_solid_stack_level_) {
      (static_cast<AnalysisDevice*>(getDevice()))->SetForceNotSolid(false);
      force_not_solid_stack_level_ = kNoLayer;
    }
    if (saved_stack_size_ < force_not_transparent_stack_level_) {
      (static_cast<AnalysisDevice*>(getDevice()))->SetForceNotTransparent(
          false);
      force_not_transparent_stack_level_ = kNoLayer;
    }
  }
}

}  // namespace skia


