// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/painter.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"

namespace views {

namespace {

class GradientPainter : public Painter {
 public:
  GradientPainter(bool horizontal,
                  SkColor* colors,
                  SkScalar* pos,
                  size_t count)
      : horizontal_(horizontal),
        count_(count) {
    pos_.reset(new SkScalar[count_]);
    colors_.reset(new SkColor[count_]);

    for (size_t i = 0; i < count_; ++i) {
      pos_[i] = pos[i];
      colors_[i] = colors[i];
    }
  }

  virtual ~GradientPainter() {}

  // Overridden from Painter:
  virtual void Paint(gfx::Canvas* canvas, const gfx::Size& size) OVERRIDE {
    SkPaint paint;
    SkPoint p[2];
    p[0].iset(0, 0);
    if (horizontal_)
      p[1].iset(size.width(), 0);
    else
      p[1].iset(0, size.height());

    skia::RefPtr<SkShader> s = skia::AdoptRef(SkGradientShader::CreateLinear(
        p, colors_.get(), pos_.get(), count_, SkShader::kClamp_TileMode, NULL));
    paint.setStyle(SkPaint::kFill_Style);
    paint.setShader(s.get());
    // Need to unref shader, otherwise never deleted.

    canvas->sk_canvas()->drawRectCoords(SkIntToScalar(0), SkIntToScalar(0),
                                        SkIntToScalar(size.width()),
                                        SkIntToScalar(size.height()), paint);
  }

 private:
  // If |horizontal_| is true then the gradiant is painted horizontally.
  bool horizontal_;
  // The gradient colors.
  scoped_array<SkColor> colors_;
  // The relative positions of the corresponding gradient colors.
  scoped_array<SkScalar> pos_;
  // The number of elements in |colors_| and |pos_|.
  size_t count_;

  DISALLOW_COPY_AND_ASSIGN(GradientPainter);
};


class ImagePainter : public Painter {
 public:
  ImagePainter(const gfx::ImageSkia& image,
               const gfx::Insets& insets,
               bool paint_center)
      : image_(image),
        insets_(insets),
        paint_center_(paint_center) {
    DCHECK(image.width() > insets.width() &&
           image.height() > insets.height());
  }

  // Paints the images.
  virtual void Paint(gfx::Canvas* canvas, const gfx::Size& size) OVERRIDE {
    if (size.width() == image_.width() && size.height() == image_.height()) {
      // Early out if the size we're to render at equals the size of the image.
      canvas->DrawImageInt(image_, 0, 0);
      return;
    }
    // Upper left.
    canvas->DrawImageInt(image_, 0, 0, insets_.left(), insets_.top(),
                         0, 0, insets_.left(), insets_.top(), true);
    // Top edge.
    canvas->DrawImageInt(
        image_,
        insets_.left(), 0, image_.width() - insets_.width(), insets_.top(),
        insets_.left(), 0, size.width() - insets_.width(), insets_.top(), true);
    // Upper right.
    canvas->DrawImageInt(
        image_,
        image_.width() - insets_.right(), 0, insets_.right(), insets_.top(),
        size.width() - insets_.right(), 0, insets_.right(), insets_.top(),
        true);
    // Right edge.
    canvas->DrawImageInt(
        image_,
        image_.width() - insets_.right(), insets_.top(),
        insets_.right(), image_.height() - insets_.height(),
        size.width() - insets_.right(), insets_.top(), insets_.right(),
        size.height() - insets_.height(), true);
    // Bottom right.
    canvas->DrawImageInt(
        image_,
        image_.width() - insets_.right(), image_.height() - insets_.bottom(),
        insets_.right(), insets_.bottom(),
        size.width() - insets_.right(),
        size.height() - insets_.bottom(), insets_.right(),
        insets_.bottom(), true);
    // Bottom edge.
    canvas->DrawImageInt(
        image_,
        insets_.left(), image_.height() - insets_.bottom(),
        image_.width() - insets_.width(), insets_.bottom(),
        insets_.left(), size.height() - insets_.bottom(),
        size.width() - insets_.width(),
        insets_.bottom(), true);
    // Bottom left.
    canvas->DrawImageInt(
        image_,
        0, image_.height() - insets_.bottom(), insets_.left(),
        insets_.bottom(),
        0, size.height() - insets_.bottom(), insets_.left(), insets_.bottom(),
        true);
    // Left.
    canvas->DrawImageInt(
        image_,
        0, insets_.top(), insets_.left(), image_.height() - insets_.height(),
        0, insets_.top(), insets_.left(), size.height() - insets_.height(),
        true);
    // Center.
    if (paint_center_) {
      canvas->DrawImageInt(
          image_,
          insets_.left(), insets_.top(),
          image_.width() - insets_.width(), image_.height() - insets_.height(),
          insets_.left(), insets_.top(),
          size.width() - insets_.width(), size.height() - insets_.height(),
          true);
    }
  }

 private:
  const gfx::ImageSkia image_;
  const gfx::Insets insets_;
  bool paint_center_;

  DISALLOW_COPY_AND_ASSIGN(ImagePainter);
};

}  // namespace

// static
void Painter::PaintPainterAt(gfx::Canvas* canvas,
                             Painter* painter,
                             const gfx::Rect& rect) {
  DCHECK(canvas && painter);
  canvas->Save();
  canvas->Translate(rect.OffsetFromOrigin());
  painter->Paint(canvas, rect.size());
  canvas->Restore();
}

// static
Painter* Painter::CreateHorizontalGradient(SkColor c1, SkColor c2) {
  SkColor colors[2];
  colors[0] = c1;
  colors[1] = c2;
  SkScalar pos[] = {0, 1};
  return new GradientPainter(true, colors, pos, 2);
}

// static
Painter* Painter::CreateVerticalGradient(SkColor c1, SkColor c2) {
  SkColor colors[2];
  colors[0] = c1;
  colors[1] = c2;
  SkScalar pos[] = {0, 1};
  return new GradientPainter(false, colors, pos, 2);
}

// static
Painter* Painter::CreateVerticalMultiColorGradient(SkColor* colors,
                                                   SkScalar* pos,
                                                   size_t count) {
  return new GradientPainter(false, colors, pos, count);
}

// static
Painter* Painter::CreateImagePainter(const gfx::ImageSkia& image,
                                     const gfx::Insets& insets,
                                     bool paint_center) {
  return new ImagePainter(image, insets, paint_center);
}

HorizontalPainter::HorizontalPainter(const int image_resource_names[]) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  for (int i = 0; i < 3; ++i)
    images_[i] = rb.GetImageNamed(image_resource_names[i]).ToImageSkia();
  height_ = images_[LEFT]->height();
  DCHECK(images_[LEFT]->height() == images_[RIGHT]->height() &&
         images_[LEFT]->height() == images_[CENTER]->height());
}

void HorizontalPainter::Paint(gfx::Canvas* canvas, const gfx::Size& size) {
  if (size.width() < (images_[LEFT]->width() + images_[CENTER]->width() +
      images_[RIGHT]->width())) {
    // No room to paint.
    return;
  }
  canvas->DrawImageInt(*images_[LEFT], 0, 0);
  canvas->DrawImageInt(*images_[RIGHT],
                       size.width() - images_[RIGHT]->width(), 0);
  canvas->TileImageInt(*images_[CENTER], images_[LEFT]->width(), 0,
      size.width() - images_[LEFT]->width() - images_[RIGHT]->width(), height_);
}

}  // namespace views
