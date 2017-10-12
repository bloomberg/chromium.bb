// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/PlaceholderImage.h"

#include "platform/geometry/FloatRect.h"
#include "platform/geometry/IntPoint.h"
#include "platform/geometry/IntRect.h"
#include "platform/graphics/BitmapImage.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/ImageObserver.h"
#include "platform/graphics/paint/PaintCanvas.h"
#include "platform/graphics/paint/PaintFlags.h"
#include "platform/graphics/paint/PaintRecord.h"
#include "platform/graphics/paint/PaintRecorder.h"
#include "platform/wtf/StdLibExtras.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkSize.h"

namespace blink {

PlaceholderImage::PlaceholderImage(ImageObserver* observer, const IntSize& size)
    : Image(observer), size_(size) {}

PlaceholderImage::~PlaceholderImage() {}

PaintImage PlaceholderImage::PaintImageForCurrentFrame() {
  auto builder = CreatePaintImageBuilder().set_completion_state(
      PaintImage::CompletionState::DONE);

  const IntRect dest_rect(0, 0, size_.Width(), size_.Height());
  if (paint_record_for_current_frame_) {
    return builder
        .set_paint_record(paint_record_for_current_frame_, dest_rect,
                          paint_record_content_id_)
        .TakePaintImage();
  }

  PaintRecorder paint_recorder;
  Draw(paint_recorder.beginRecording(FloatRect(dest_rect)), PaintFlags(),
       FloatRect(dest_rect), FloatRect(dest_rect),
       kDoNotRespectImageOrientation, kClampImageToSourceRect, kSyncDecode);

  paint_record_for_current_frame_ = paint_recorder.finishRecordingAsPicture();
  paint_record_content_id_ = PaintImage::GetNextContentId();
  return builder
      .set_paint_record(paint_record_for_current_frame_, dest_rect,
                        paint_record_content_id_)
      .TakePaintImage();
}

void PlaceholderImage::Draw(PaintCanvas* canvas,
                            const PaintFlags& base_flags,
                            const FloatRect& dest_rect,
                            const FloatRect& src_rect,
                            RespectImageOrientationEnum respect_orientation,
                            ImageClampingMode image_clamping_mode,
                            ImageDecodingMode decode_mode) {
  if (!src_rect.Intersects(FloatRect(0.0f, 0.0f,
                                     static_cast<float>(size_.Width()),
                                     static_cast<float>(size_.Height())))) {
    return;
  }

  // Placeholder image visual specifications:
  // https://docs.google.com/document/d/1BHeA1azbgCdZgCnr16VN2g7A9MHPQ_dwKn5szh8evMQ/edit

  PaintFlags flags(base_flags);
  flags.setStyle(PaintFlags::kFill_Style);
  flags.setColor(SkColorSetARGB(0x80, 0xD9, 0xD9, 0xD9));
  canvas->drawRect(dest_rect, flags);

  constexpr int kIconWidth = 24;
  constexpr int kIconHeight = 24;
  constexpr int kIconPaddingX = 8;
  constexpr int kIconPaddingY = 5;

  if (dest_rect.Width() < kIconWidth + 2 * kIconPaddingX ||
      dest_rect.Height() < kIconHeight + 2 * kIconPaddingY) {
    return;
  }

  DEFINE_STATIC_REF(Image, icon_image,
                    (Image::LoadPlatformResource("placeholderIcon")));
  DCHECK(!icon_image->IsNull());

  FloatRect icon_dest_rect(
      dest_rect.X() + (dest_rect.Width() - kIconWidth) / 2.0f,
      dest_rect.Y() + (dest_rect.Height() - kIconHeight) / 2.0f, kIconWidth,
      kIconHeight);

  // Note that the |icon_image| is not scaled according to dest_rect / src_rect,
  // and is always drawn at the same size. This is so that placeholder icons are
  // visible (e.g. when replacing a large image that's scaled down to a small
  // area) and so that all placeholder images on the same page look consistent.
  canvas->drawImageRect(icon_image->PaintImageForCurrentFrame(),
                        IntRect(IntPoint::Zero(), icon_image->Size()),
                        icon_dest_rect, &base_flags,
                        PaintCanvas::kFast_SrcRectConstraint);
}

void PlaceholderImage::DrawPattern(GraphicsContext& context,
                                   const FloatRect& src_rect,
                                   const FloatSize& scale,
                                   const FloatPoint& phase,
                                   SkBlendMode mode,
                                   const FloatRect& dest_rect,
                                   const FloatSize& repeat_spacing) {
  DCHECK(context.Canvas());

  PaintFlags flags = context.FillFlags();
  flags.setBlendMode(mode);

  // Ignore the pattern specifications and just draw a single placeholder image
  // over the whole |dest_rect|. This is done in order to prevent repeated icons
  // from cluttering tiled background images.
  Draw(context.Canvas(), flags, dest_rect, src_rect,
       kDoNotRespectImageOrientation, kClampImageToSourceRect,
       kUnspecifiedDecode);
}

void PlaceholderImage::DestroyDecodedData() {
  paint_record_for_current_frame_.reset();
}

}  // namespace blink
