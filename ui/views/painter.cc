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
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"


namespace views {

namespace {

// GradientPainter ------------------------------------------------------------

class GradientPainter : public Painter {
 public:
  GradientPainter(bool horizontal,
                  SkColor* colors,
                  SkScalar* pos,
                  size_t count);
  virtual ~GradientPainter();

  // Painter:
  virtual gfx::Size GetMinimumSize() const OVERRIDE;
  virtual void Paint(gfx::Canvas* canvas, const gfx::Size& size) OVERRIDE;

 private:
  // If |horizontal_| is true then the gradient is painted horizontally.
  bool horizontal_;
  // The gradient colors.
  scoped_ptr<SkColor[]> colors_;
  // The relative positions of the corresponding gradient colors.
  scoped_ptr<SkScalar[]> pos_;
  // The number of elements in |colors_| and |pos_|.
  size_t count_;

  DISALLOW_COPY_AND_ASSIGN(GradientPainter);
};

GradientPainter::GradientPainter(bool horizontal,
                                 SkColor* colors,
                                 SkScalar* pos,
                                 size_t count)
    : horizontal_(horizontal),
      colors_(new SkColor[count]),
      pos_(new SkScalar[count]),
      count_(count) {
  for (size_t i = 0; i < count_; ++i) {
    pos_[i] = pos[i];
    colors_[i] = colors[i];
  }
}

GradientPainter::~GradientPainter() {
}

gfx::Size GradientPainter::GetMinimumSize() const {
  return gfx::Size();
}

void GradientPainter::Paint(gfx::Canvas* canvas, const gfx::Size& size) {
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

  canvas->sk_canvas()->drawRectCoords(SkIntToScalar(0), SkIntToScalar(0),
                                      SkIntToScalar(size.width()),
                                      SkIntToScalar(size.height()), paint);
}


// ImagePainter ---------------------------------------------------------------

// ImagePainter stores and paints nine images as a scalable grid.
class VIEWS_EXPORT ImagePainter : public Painter {
 public:
  // Constructs an ImagePainter with the specified image resource ids.
  // See CreateImageGridPainter()'s comment regarding image ID count and order.
  explicit ImagePainter(const int image_ids[]);

  // Constructs an ImagePainter with the specified image and insets.
  ImagePainter(const gfx::ImageSkia& image, const gfx::Insets& insets);

  virtual ~ImagePainter();

  // Returns true if the images are empty.
  bool IsEmpty() const;

  // Painter:
  virtual gfx::Size GetMinimumSize() const OVERRIDE;
  virtual void Paint(gfx::Canvas* canvas, const gfx::Size& size) OVERRIDE;

 private:
  // Stretches the given image over the specified canvas area.
  static void Fill(gfx::Canvas* c,
                   const gfx::ImageSkia& i,
                   int x,
                   int y,
                   int w,
                   int h);

  // Images are numbered as depicted below.
  //  ____________________
  // |__i0__|__i1__|__i2__|
  // |__i3__|__i4__|__i5__|
  // |__i6__|__i7__|__i8__|
  gfx::ImageSkia images_[9];

  DISALLOW_COPY_AND_ASSIGN(ImagePainter);
};

ImagePainter::ImagePainter(const int image_ids[]) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  for (size_t i = 0; i < 9; ++i)
    if (image_ids[i] != 0)
      images_[i] = *rb.GetImageSkiaNamed(image_ids[i]);
}

ImagePainter::ImagePainter(const gfx::ImageSkia& image,
                           const gfx::Insets& insets) {
  DCHECK_GE(image.width(), insets.width());
  DCHECK_GE(image.height(), insets.height());

  // Extract subsets of the original image to match the |images_| format.
  const int x[] =
      { 0, insets.left(), image.width() - insets.right(), image.width() };
  const int y[] =
      { 0, insets.top(), image.height() - insets.bottom(), image.height() };

  for (size_t j = 0; j < 3; ++j) {
    for (size_t i = 0; i < 3; ++i) {
      images_[i + j * 3] = gfx::ImageSkiaOperations::ExtractSubset(image,
          gfx::Rect(x[i], y[j], x[i + 1] - x[i], y[j + 1] - y[j]));
    }
  }
}

ImagePainter::~ImagePainter() {
}

bool ImagePainter::IsEmpty() const {
  return images_[0].isNull();
}

gfx::Size ImagePainter::GetMinimumSize() const {
  return IsEmpty() ? gfx::Size() : gfx::Size(
      images_[0].width() + images_[1].width() + images_[2].width(),
      images_[0].height() + images_[3].height() + images_[6].height());
}

void ImagePainter::Paint(gfx::Canvas* canvas, const gfx::Size& size) {
  if (IsEmpty())
    return;

  // In case the corners and edges don't all have the same width/height, we draw
  // the center first, and extend it out in all directions to the edges of the
  // images with the smallest widths/heights.  This way there will be no
  // unpainted areas, though some corners or edges might overlap the center.
  int w = size.width();
  int i0w = images_[0].width();
  int i2w = images_[2].width();
  int i3w = images_[3].width();
  int i5w = images_[5].width();
  int i6w = images_[6].width();
  int i8w = images_[8].width();
  int i4x = std::min(std::min(i0w, i3w), i6w);
  int i4w = w - i4x - std::min(std::min(i2w, i5w), i8w);
  int h = size.height();
  int i0h = images_[0].height();
  int i1h = images_[1].height();
  int i2h = images_[2].height();
  int i6h = images_[6].height();
  int i7h = images_[7].height();
  int i8h = images_[8].height();
  int i4y = std::min(std::min(i0h, i1h), i2h);
  int i4h = h - i4y - std::min(std::min(i6h, i7h), i8h);
  if (!images_[4].isNull())
    Fill(canvas, images_[4], i4x, i4y, i4w, i4h);
  canvas->DrawImageInt(images_[0], 0, 0);
  Fill(canvas, images_[1], i0w, 0, w - i0w - i2w, i1h);
  canvas->DrawImageInt(images_[2], w - i2w, 0);
  Fill(canvas, images_[3], 0, i0h, i3w, h - i0h - i6h);
  Fill(canvas, images_[5], w - i5w, i2h, i5w, h - i2h - i8h);
  canvas->DrawImageInt(images_[6], 0, h - i6h);
  Fill(canvas, images_[7], i6w, h - i7h, w - i6w - i8w, i7h);
  canvas->DrawImageInt(images_[8], w - i8w, h - i8h);
}

// static
void ImagePainter::Fill(gfx::Canvas* c,
                        const gfx::ImageSkia& i,
                        int x,
                        int y,
                        int w,
                        int h) {
  c->DrawImageInt(i, 0, 0, i.width(), i.height(), x, y, w, h, false);
}

}  // namespace


// Painter --------------------------------------------------------------------

Painter::Painter() {
}

Painter::~Painter() {
}

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
                                     const gfx::Insets& insets) {
  return new ImagePainter(image, insets);
}

// static
Painter* Painter::CreateImageGridPainter(const int image_ids[]) {
  return new ImagePainter(image_ids);
}


// HorizontalPainter ----------------------------------------------------------

HorizontalPainter::HorizontalPainter(const int image_resource_names[]) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  for (int i = 0; i < 3; ++i)
    images_[i] = rb.GetImageNamed(image_resource_names[i]).ToImageSkia();
  DCHECK_EQ(images_[LEFT]->height(), images_[CENTER]->height());
  DCHECK_EQ(images_[LEFT]->height(), images_[RIGHT]->height());
}

HorizontalPainter::~HorizontalPainter() {
}

gfx::Size HorizontalPainter::GetMinimumSize() const {
  return gfx::Size(
      images_[LEFT]->width() + images_[CENTER]->width() +
          images_[RIGHT]->width(), images_[LEFT]->height());
}

void HorizontalPainter::Paint(gfx::Canvas* canvas, const gfx::Size& size) {
  if (size.width() < GetMinimumSize().width())
    return;  // No room to paint.

  canvas->DrawImageInt(*images_[LEFT], 0, 0);
  canvas->DrawImageInt(*images_[RIGHT], size.width() - images_[RIGHT]->width(),
                       0);
  canvas->TileImageInt(
      *images_[CENTER], images_[LEFT]->width(), 0,
      size.width() - images_[LEFT]->width() - images_[RIGHT]->width(),
      images_[LEFT]->height());
}

}  // namespace views
