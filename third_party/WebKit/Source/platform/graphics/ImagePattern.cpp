// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/ImagePattern.h"

#include "platform/graphics/Image.h"
#include "platform/graphics/paint/PaintShader.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

PassRefPtr<ImagePattern> ImagePattern::Create(PassRefPtr<Image> image,
                                              RepeatMode repeat_mode) {
  return AdoptRef(new ImagePattern(std::move(image), repeat_mode));
}

ImagePattern::ImagePattern(PassRefPtr<Image> image, RepeatMode repeat_mode)
    : Pattern(repeat_mode), tile_image_(image->ImageForCurrentFrame()) {
  previous_local_matrix_.setIdentity();
  if (tile_image_) {
    // TODO(fmalita): mechanism to extract the actual SkImageInfo from an
    // SkImage?
    const SkImageInfo info = SkImageInfo::MakeN32Premul(
        tile_image_->width() + (IsRepeatX() ? 0 : 2),
        tile_image_->height() + (IsRepeatY() ? 0 : 2));
    AdjustExternalMemoryAllocated(info.getSafeSize(info.minRowBytes()));
  }
}

bool ImagePattern::IsLocalMatrixChanged(const SkMatrix& local_matrix) const {
  if (IsRepeatXY())
    return Pattern::IsLocalMatrixChanged(local_matrix);
  return local_matrix != previous_local_matrix_;
}

sk_sp<PaintShader> ImagePattern::CreateShader(const SkMatrix& local_matrix) {
  if (!tile_image_)
    return WrapSkShader(SkShader::MakeColorShader(SK_ColorTRANSPARENT));

  if (IsRepeatXY()) {
    // Fast path: for repeatXY we just return a shader from the original image.
    return MakePaintShaderImage(tile_image_, SkShader::kRepeat_TileMode,
                                SkShader::kRepeat_TileMode, &local_matrix);
  }

  // Skia does not have a "draw the tile only once" option. Clamp_TileMode
  // repeats the last line of the image after drawing one tile. To avoid
  // filling the space with arbitrary pixels, this workaround forces the
  // image to have a line of transparent pixels on the "repeated" edge(s),
  // thus causing extra space to be transparent filled.
  SkShader::TileMode tile_mode_x =
      IsRepeatX() ? SkShader::kRepeat_TileMode : SkShader::kClamp_TileMode;
  SkShader::TileMode tile_mode_y =
      IsRepeatY() ? SkShader::kRepeat_TileMode : SkShader::kClamp_TileMode;
  int border_pixel_x = IsRepeatX() ? 0 : 1;
  int border_pixel_y = IsRepeatY() ? 0 : 1;

  // Create a transparent image 2 pixels wider and/or taller than the
  // original, then copy the orignal into the middle of it.
  // FIXME: Is there a better way to pad (not scale) an image in skia?
  sk_sp<SkSurface> surface = SkSurface::MakeRasterN32Premul(
      tile_image_->width() + 2 * border_pixel_x,
      tile_image_->height() + 2 * border_pixel_y);
  if (!surface)
    return WrapSkShader(SkShader::MakeColorShader(SK_ColorTRANSPARENT));

  SkPaint paint;
  paint.setBlendMode(SkBlendMode::kSrc);
  surface->getCanvas()->drawImage(tile_image_, border_pixel_x, border_pixel_y,
                                  &paint);

  previous_local_matrix_ = local_matrix;
  SkMatrix adjusted_matrix(local_matrix);
  adjusted_matrix.postTranslate(-border_pixel_x, -border_pixel_y);

  return MakePaintShaderImage(surface->makeImageSnapshot(), tile_mode_x,
                              tile_mode_y, &adjusted_matrix);
}

bool ImagePattern::IsTextureBacked() const {
  return tile_image_ && tile_image_->isTextureBacked();
}

}  // namespace blink
