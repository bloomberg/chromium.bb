/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/DragImage.h"

#include <algorithm>
#include <memory>
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/fonts/Font.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/FontMetrics.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/IntPoint.h"
#include "platform/graphics/BitmapImage.h"
#include "platform/graphics/Color.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/Image.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/text/BidiTextRun.h"
#include "platform/text/StringTruncator.h"
#include "platform/text/TextRun.h"
#include "platform/transforms/AffineTransform.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/text/WTFString.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorSpaceXformCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

namespace {

const float kDragLabelBorderX = 4;
// Keep border_y in synch with DragController::LinkDragBorderInset.
const float kDragLabelBorderY = 2;
const float kLabelBorderYOffset = 2;

const float kMaxDragLabelWidth = 300;
const float kMaxDragLabelStringWidth =
    (kMaxDragLabelWidth - 2 * kDragLabelBorderX);

const float kDragLinkLabelFontSize = 11;
const float kDragLinkUrlFontSize = 10;

}  // anonymous namespace

PaintImage DragImage::ResizeAndOrientImage(
    const PaintImage& image,
    ImageOrientation orientation,
    FloatSize image_scale,
    float opacity,
    InterpolationQuality interpolation_quality) {
  IntSize size(image.sk_image()->width(), image.sk_image()->height());
  size.Scale(image_scale.Width(), image_scale.Height());
  AffineTransform transform;
  if (orientation != kDefaultImageOrientation) {
    if (orientation.UsesWidthAsHeight())
      size = size.TransposedSize();
    transform *= orientation.TransformFromDefault(FloatSize(size));
  }
  transform.ScaleNonUniform(image_scale.Width(), image_scale.Height());

  if (size.IsEmpty())
    return PaintImage();

  if (transform.IsIdentity() && opacity == 1) {
    // Nothing to adjust, just use the original.
    DCHECK_EQ(image.sk_image()->width(), size.Width());
    DCHECK_EQ(image.sk_image()->height(), size.Height());
    return image;
  }

  sk_sp<SkSurface> surface =
      SkSurface::MakeRasterN32Premul(size.Width(), size.Height());
  if (!surface)
    return PaintImage();

  SkPaint paint;
  DCHECK_GE(opacity, 0);
  DCHECK_LE(opacity, 1);
  paint.setAlpha(opacity * 255);
  paint.setFilterQuality(interpolation_quality == kInterpolationNone
                             ? kNone_SkFilterQuality
                             : kHigh_SkFilterQuality);

  SkCanvas* canvas = surface->getCanvas();
  std::unique_ptr<SkCanvas> color_transform_canvas;
  if (RuntimeEnabledFeatures::ColorCorrectRenderingEnabled()) {
    color_transform_canvas =
        SkCreateColorSpaceXformCanvas(canvas, SkColorSpace::MakeSRGB());
    canvas = color_transform_canvas.get();
  }
  canvas->concat(AffineTransformToSkMatrix(transform));
  canvas->drawImage(image.sk_image(), 0, 0, &paint);

  return image.CloneWithSkImage(surface->makeImageSnapshot());
}

FloatSize DragImage::ClampedImageScale(const IntSize& image_size,
                                       const IntSize& size,
                                       const IntSize& max_size) {
  // Non-uniform scaling for size mapping.
  FloatSize image_scale(
      static_cast<float>(size.Width()) / image_size.Width(),
      static_cast<float>(size.Height()) / image_size.Height());

  // Uniform scaling for clamping.
  const float clamp_scale_x =
      size.Width() > max_size.Width()
          ? static_cast<float>(max_size.Width()) / size.Width()
          : 1;
  const float clamp_scale_y =
      size.Height() > max_size.Height()
          ? static_cast<float>(max_size.Height()) / size.Height()
          : 1;
  image_scale.Scale(std::min(clamp_scale_x, clamp_scale_y));

  return image_scale;
}

std::unique_ptr<DragImage> DragImage::Create(
    Image* image,
    RespectImageOrientationEnum should_respect_image_orientation,
    float device_scale_factor,
    InterpolationQuality interpolation_quality,
    float opacity,
    FloatSize image_scale) {
  if (!image)
    return nullptr;

  PaintImage paint_image = image->PaintImageForCurrentFrame();
  if (!paint_image)
    return nullptr;

  ImageOrientation orientation;
  if (should_respect_image_orientation == kRespectImageOrientation &&
      image->IsBitmapImage())
    orientation = ToBitmapImage(image)->CurrentFrameOrientation();

  SkBitmap bm;
  paint_image = ResizeAndOrientImage(paint_image, orientation, image_scale,
                                     opacity, interpolation_quality);
  if (!paint_image || !paint_image.sk_image()->asLegacyBitmap(
                          &bm, SkImage::kRO_LegacyBitmapMode)) {
    return nullptr;
  }

  return WTF::WrapUnique(
      new DragImage(bm, device_scale_factor, interpolation_quality));
}

static Font DeriveDragLabelFont(int size,
                                FontWeight font_weight,
                                const FontDescription& system_font) {
  FontDescription description = system_font;
  description.SetWeight(font_weight);
  description.SetSpecifiedSize(size);
  description.SetComputedSize(size);
  Font result(description);
  result.Update(nullptr);
  return result;
}

std::unique_ptr<DragImage> DragImage::Create(const KURL& url,
                                             const String& in_label,
                                             const FontDescription& system_font,
                                             float device_scale_factor) {
  const Font label_font =
      DeriveDragLabelFont(kDragLinkLabelFontSize, kFontWeightBold, system_font);
  const SimpleFontData* label_font_data = label_font.PrimaryFont();
  DCHECK(label_font_data);
  const Font url_font =
      DeriveDragLabelFont(kDragLinkUrlFontSize, kFontWeightNormal, system_font);
  const SimpleFontData* url_font_data = url_font.PrimaryFont();
  DCHECK(url_font_data);

  if (!label_font_data || !url_font_data)
    return nullptr;

  FontCachePurgePreventer font_cache_purge_preventer;

  bool draw_url_string = true;
  bool clip_url_string = false;
  bool clip_label_string = false;
  float max_drag_label_string_width_dip =
      kMaxDragLabelStringWidth / device_scale_factor;

  String url_string = url.GetString();
  String label = in_label.StripWhiteSpace();
  if (label.IsEmpty()) {
    draw_url_string = false;
    label = url_string;
  }

  // First step is drawing the link drag image width.
  TextRun label_run(label.Impl());
  TextRun url_run(url_string.Impl());
  IntSize label_size(label_font.Width(label_run),
                     label_font_data->GetFontMetrics().Ascent() +
                         label_font_data->GetFontMetrics().Descent());

  if (label_size.Width() > max_drag_label_string_width_dip) {
    label_size.SetWidth(max_drag_label_string_width_dip);
    clip_label_string = true;
  }

  IntSize url_string_size;
  IntSize image_size(label_size.Width() + kDragLabelBorderX * 2,
                     label_size.Height() + kDragLabelBorderY * 2);

  if (draw_url_string) {
    url_string_size.SetWidth(url_font.Width(url_run));
    url_string_size.SetHeight(url_font_data->GetFontMetrics().Ascent() +
                              url_font_data->GetFontMetrics().Descent());
    image_size.SetHeight(image_size.Height() + url_string_size.Height());
    if (url_string_size.Width() > max_drag_label_string_width_dip) {
      image_size.SetWidth(max_drag_label_string_width_dip);
      clip_url_string = true;
    } else
      image_size.SetWidth(
          std::max(label_size.Width(), url_string_size.Width()) +
          kDragLabelBorderX * 2);
  }

  // We now know how big the image needs to be, so we create and
  // fill the background
  IntSize scaled_image_size = image_size;
  scaled_image_size.Scale(device_scale_factor);
  std::unique_ptr<ImageBuffer> buffer(ImageBuffer::Create(scaled_image_size));
  if (!buffer)
    return nullptr;

  buffer->Canvas()->scale(device_scale_factor, device_scale_factor);

  const float kDragLabelRadius = 5;

  IntRect rect(IntPoint(), image_size);
  PaintFlags background_paint;
  background_paint.setColor(SkColorSetRGB(140, 140, 140));
  background_paint.setAntiAlias(true);
  SkRRect rrect;
  rrect.setRectXY(SkRect::MakeWH(image_size.Width(), image_size.Height()),
                  kDragLabelRadius, kDragLabelRadius);
  buffer->Canvas()->drawRRect(rrect, background_paint);

  // Draw the text
  PaintFlags text_paint;
  if (draw_url_string) {
    if (clip_url_string)
      url_string = StringTruncator::CenterTruncate(
          url_string, image_size.Width() - (kDragLabelBorderX * 2.0f),
          url_font);
    IntPoint text_pos(
        kDragLabelBorderX,
        image_size.Height() -
            (kLabelBorderYOffset + url_font_data->GetFontMetrics().Descent()));
    TextRun text_run(url_string);
    url_font.DrawText(buffer->Canvas(), TextRunPaintInfo(text_run), text_pos,
                      device_scale_factor, text_paint);
  }

  if (clip_label_string)
    label = StringTruncator::RightTruncate(
        label, image_size.Width() - (kDragLabelBorderX * 2.0f), label_font);

  bool has_strong_directionality;
  TextRun text_run =
      TextRunWithDirectionality(label, &has_strong_directionality);
  IntPoint text_pos(
      kDragLabelBorderX,
      kDragLabelBorderY + label_font.GetFontDescription().ComputedPixelSize());
  if (has_strong_directionality &&
      text_run.Direction() == TextDirection::kRtl) {
    float text_width = label_font.Width(text_run);
    int available_width = image_size.Width() - kDragLabelBorderX * 2;
    text_pos.SetX(available_width - ceilf(text_width));
  }
  label_font.DrawBidiText(buffer->Canvas(), TextRunPaintInfo(text_run),
                          FloatPoint(text_pos), Font::kDoNotPaintIfFontNotReady,
                          device_scale_factor, text_paint);

  RefPtr<Image> image = buffer->NewImageSnapshot();
  return DragImage::Create(image.Get(), kDoNotRespectImageOrientation,
                           device_scale_factor);
}

DragImage::DragImage(const SkBitmap& bitmap,
                     float resolution_scale,
                     InterpolationQuality interpolation_quality)
    : bitmap_(bitmap),
      resolution_scale_(resolution_scale),
      interpolation_quality_(interpolation_quality) {}

DragImage::~DragImage() {}

void DragImage::Scale(float scale_x, float scale_y) {
  skia::ImageOperations::ResizeMethod resize_method =
      interpolation_quality_ == kInterpolationNone
          ? skia::ImageOperations::RESIZE_BOX
          : skia::ImageOperations::RESIZE_LANCZOS3;
  int image_width = scale_x * bitmap_.width();
  int image_height = scale_y * bitmap_.height();
  bitmap_ = skia::ImageOperations::Resize(bitmap_, resize_method, image_width,
                                          image_height);
}

}  // namespace blink
