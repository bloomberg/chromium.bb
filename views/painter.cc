// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/painter.h"

#include "base/logging.h"
#include "gfx/canvas.h"
#include "gfx/canvas_skia.h"
#include "gfx/insets.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/base/resource/resource_bundle.h"

namespace views {

namespace {

class GradientPainter : public Painter {
 public:
  GradientPainter(bool horizontal, const SkColor& top, const SkColor& bottom)
      : horizontal_(horizontal) {
    colors_[0] = top;
    colors_[1] = bottom;
  }

  virtual ~GradientPainter() {
  }

  void Paint(int w, int h, gfx::Canvas* canvas) {
    SkPaint paint;
    SkPoint p[2];
    p[0].set(SkIntToScalar(0), SkIntToScalar(0));
    if (horizontal_)
      p[1].set(SkIntToScalar(w), SkIntToScalar(0));
    else
      p[1].set(SkIntToScalar(0), SkIntToScalar(h));

    SkShader* s =
        SkGradientShader::CreateLinear(p, colors_, NULL, 2,
                                       SkShader::kClamp_TileMode, NULL);
    paint.setStyle(SkPaint::kFill_Style);
    paint.setShader(s);
    // Need to unref shader, otherwise never deleted.
    s->unref();

    canvas->AsCanvasSkia()->drawRectCoords(
        SkIntToScalar(0), SkIntToScalar(0), SkIntToScalar(w), SkIntToScalar(h),
        paint);
  }

 private:
  bool horizontal_;
  SkColor colors_[2];

  DISALLOW_COPY_AND_ASSIGN(GradientPainter);
};


class ImagePainter : public Painter {
 public:
  ImagePainter(const SkBitmap& image,
               const gfx::Insets& insets,
               bool paint_center)
      : image_(image),
        insets_(insets),
        paint_center_(paint_center) {
    DCHECK(image.width() > insets.width() &&
           image.height() > insets.height());
  }

  // Paints the images.
  virtual void Paint(int w, int h, gfx::Canvas* canvas) {
    if (w == image_.width() && h == image_.height()) {
      // Early out if the size we're to render at equals the size of the image.
      canvas->DrawBitmapInt(image_, 0, 0);
      return;
    }
    // Upper left.
    canvas->DrawBitmapInt(image_, 0, 0, insets_.left(), insets_.top(),
                          0, 0, insets_.left(), insets_.top(), true);
    // Top edge.
    canvas->DrawBitmapInt(
        image_,
        insets_.left(), 0, image_.width() - insets_.width(), insets_.top(),
        insets_.left(), 0, w - insets_.width(), insets_.top(), true);
    // Upper right.
    canvas->DrawBitmapInt(
        image_,
        image_.width() - insets_.right(), 0, insets_.right(), insets_.top(),
        w - insets_.right(), 0, insets_.right(), insets_.top(), true);
    // Right edge.
    canvas->DrawBitmapInt(
        image_,
        image_.width() - insets_.right(), insets_.top(),
        insets_.right(), image_.height() - insets_.height(),
        w - insets_.right(), insets_.top(), insets_.right(),
        h - insets_.height(), true);
    // Bottom right.
    canvas->DrawBitmapInt(
        image_,
        image_.width() - insets_.right(), image_.height() - insets_.bottom(),
        insets_.right(), insets_.bottom(),
        w - insets_.right(), h - insets_.bottom(), insets_.right(),
        insets_.bottom(), true);
    // Bottom edge.
    canvas->DrawBitmapInt(
        image_,
        insets_.left(), image_.height() - insets_.bottom(),
        image_.width() - insets_.width(), insets_.bottom(),
        insets_.left(), h - insets_.bottom(), w - insets_.width(),
        insets_.bottom(), true);
    // Bottom left.
    canvas->DrawBitmapInt(
        image_,
        0, image_.height() - insets_.bottom(), insets_.left(),
        insets_.bottom(),
        0, h - insets_.bottom(), insets_.left(), insets_.bottom(), true);
    // Left.
    canvas->DrawBitmapInt(
        image_,
        0, insets_.top(), insets_.left(), image_.height() - insets_.height(),
        0, insets_.top(), insets_.left(), h - insets_.height(), true);
    // Center.
    if (paint_center_) {
      canvas->DrawBitmapInt(
          image_,
          insets_.left(), insets_.top(),
          image_.width() - insets_.width(), image_.height() - insets_.height(),
          insets_.left(), insets_.top(),
          w - insets_.width(), h - insets_.height(), true);
    }
  }

 private:
  const SkBitmap image_;
  const gfx::Insets insets_;
  bool paint_center_;

  DISALLOW_COPY_AND_ASSIGN(ImagePainter);
};

}  // namespace

// static
void Painter::PaintPainterAt(int x, int y, int w, int h,
                             gfx::Canvas* canvas, Painter* painter) {
  DCHECK(canvas && painter);
  if (w < 0 || h < 0)
    return;
  canvas->Save();
  canvas->TranslateInt(x, y);
  painter->Paint(w, h, canvas);
  canvas->Restore();
}

// static
Painter* Painter::CreateHorizontalGradient(SkColor c1, SkColor c2) {
  return new GradientPainter(true, c1, c2);
}

// static
Painter* Painter::CreateVerticalGradient(SkColor c1, SkColor c2) {
  return new GradientPainter(false, c1, c2);
}

// static
Painter* Painter::CreateImagePainter(const SkBitmap& image,
                                     const gfx::Insets& insets,
                                     bool paint_center) {
  return new ImagePainter(image, insets, paint_center);
}

HorizontalPainter::HorizontalPainter(const int image_resource_names[]) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  for (int i = 0; i < 3; ++i)
    images_[i] = rb.GetBitmapNamed(image_resource_names[i]);
  height_ = images_[LEFT]->height();
  DCHECK(images_[LEFT]->height() == images_[RIGHT]->height() &&
         images_[LEFT]->height() == images_[CENTER]->height());
}

void HorizontalPainter::Paint(int w, int h, gfx::Canvas* canvas) {
  if (w < (images_[LEFT]->width() + images_[CENTER]->width() +
            images_[RIGHT]->width())) {
    // No room to paint.
    return;
  }
  canvas->DrawBitmapInt(*images_[LEFT], 0, 0);
  canvas->DrawBitmapInt(*images_[RIGHT], w - images_[RIGHT]->width(), 0);
  canvas->TileImageInt(*images_[CENTER],
                       images_[LEFT]->width(),
                       0,
                       w - images_[LEFT]->width() - images_[RIGHT]->width(),
                       height_);
}

}  // namespace views
