// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/image_skia_operations.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "skia/ext/image_operations.h"
#include "skia/ext/platform_canvas.h"
#include "ui/base/layout.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/gfx/skia_util.h"

namespace gfx {
namespace {

bool ScalingEnabled() {
  static bool scale_images = !CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableScalingInImageSkiaOperations);
  return scale_images;
}

// Creates 2x scaled image of the give |source|.
ImageSkiaRep Create2XImageSkiaRep(const ImageSkiaRep& source) {
  gfx::Size size(source.GetWidth() * 2.0f, source.GetHeight() * 2.0f);
  skia::PlatformCanvas canvas(size.width(), size.height(), false);
  SkRect resized_bounds = RectToSkRect(gfx::Rect(size));
  canvas.drawBitmapRect(source.sk_bitmap(), NULL, resized_bounds);
  SkBitmap resized_bitmap = canvas.getDevice()->accessBitmap(false);
  return ImageSkiaRep(resized_bitmap, ui::SCALE_FACTOR_200P);
}

// A utility function to synchronize the scale factor of the two images.
// When the command line option "--disable-scaling-in-image-skia-operation"
// is provided, this function will fail if the scale factors of the two images
// are different. This assumes that the platform only supports
// 1x and 2x scale factors.
// TODO(oshima): Remove and replace this with plain CHECK once
// 2x images for all resources are provided.
void MatchScale(ImageSkiaRep* first, ImageSkiaRep* second) {
  if (first->scale_factor() != second->scale_factor()) {
    CHECK(ScalingEnabled());
    ImageSkiaRep* target = NULL;
    if (first->scale_factor() == ui::SCALE_FACTOR_100P) {
      target = first;
    } else {
      target = second;
    }
    *target = Create2XImageSkiaRep(*target);
  }
}

class BlendingImageSource : public gfx::ImageSkiaSource {
 public:
  BlendingImageSource(const ImageSkia& first,
                      const ImageSkia& second,
                      double alpha)
      : first_(first),
        second_(second),
        alpha_(alpha) {
  }

  // gfx::ImageSkiaSource overrides:
  virtual ImageSkiaRep GetImageForScale(ui::ScaleFactor scale_factor) OVERRIDE {
    ImageSkiaRep first_rep = first_.GetRepresentation(scale_factor);
    ImageSkiaRep second_rep = second_.GetRepresentation(scale_factor);
    MatchScale(&first_rep, &second_rep);
    SkBitmap blended = SkBitmapOperations::CreateBlendedBitmap(
        first_rep.sk_bitmap(), second_rep.sk_bitmap(), alpha_);
    return ImageSkiaRep(blended, first_rep.scale_factor());
  }

 private:
  const ImageSkia first_;
  const ImageSkia second_;
  double alpha_;

  DISALLOW_COPY_AND_ASSIGN(BlendingImageSource);
};

class MaskedImageSource : public gfx::ImageSkiaSource {
 public:
  MaskedImageSource(const ImageSkia& rgb, const ImageSkia& alpha)
      : rgb_(rgb),
        alpha_(alpha) {
  }

  // gfx::ImageSkiaSource overrides:
  virtual ImageSkiaRep GetImageForScale(ui::ScaleFactor scale_factor) OVERRIDE {
    ImageSkiaRep rgb_rep = rgb_.GetRepresentation(scale_factor);
    ImageSkiaRep alpha_rep = alpha_.GetRepresentation(scale_factor);
    MatchScale(&rgb_rep, &alpha_rep);
    return ImageSkiaRep(SkBitmapOperations::CreateMaskedBitmap(
        rgb_rep.sk_bitmap(), alpha_rep.sk_bitmap()),
                        rgb_rep.scale_factor());
  }

 private:
  const ImageSkia rgb_;
  const ImageSkia alpha_;

  DISALLOW_COPY_AND_ASSIGN(MaskedImageSource);
};

class TiledImageSource : public gfx::ImageSkiaSource {
 public:
  TiledImageSource(const ImageSkia& source,
                   int src_x, int src_y,
                   int dst_w, int dst_h)
      : source_(source),
        src_x_(src_x),
        src_y_(src_y),
        dst_w_(dst_w),
        dst_h_(dst_h) {
  }

  // gfx::ImageSkiaSource overrides:
  virtual ImageSkiaRep GetImageForScale(ui::ScaleFactor scale_factor) OVERRIDE {
    ImageSkiaRep source_rep = source_.GetRepresentation(scale_factor);
    float scale = ui::GetScaleFactorScale(source_rep.scale_factor());
    return ImageSkiaRep(
        SkBitmapOperations::CreateTiledBitmap(
            source_rep.sk_bitmap(),
            src_x_ * scale, src_y_ * scale, dst_w_ * scale, dst_h_ * scale),
        source_rep.scale_factor());
  }

 private:
  const ImageSkia& source_;
  const int src_x_;
  const int src_y_;
  const int dst_w_;
  const int dst_h_;

  DISALLOW_COPY_AND_ASSIGN(TiledImageSource);
};

// ResizeSource resizes relevant image reps in |source| to |target_dip_size|
// for requested scale factors.
class ResizeSource : public ImageSkiaSource {
 public:
  ResizeSource(const ImageSkia& source,
               const Size& target_dip_size)
      : source_(source),
        target_dip_size_(target_dip_size) {
  }
  virtual ~ResizeSource() {}

  // gfx::ImageSkiaSource overrides:
  virtual ImageSkiaRep GetImageForScale(ui::ScaleFactor scale_factor) OVERRIDE {
    const ImageSkiaRep& image_rep = source_.GetRepresentation(scale_factor);
    if (image_rep.GetWidth() == target_dip_size_.width() &&
        image_rep.GetHeight() == target_dip_size_.height())
      return image_rep;

    const float scale = ui::GetScaleFactorScale(scale_factor);
    const Size target_pixel_size(target_dip_size_.Scale(scale));
    const SkBitmap resized = skia::ImageOperations::Resize(
        image_rep.sk_bitmap(),
        skia::ImageOperations::RESIZE_BEST,
        target_pixel_size.width(),
        target_pixel_size.height());
    return ImageSkiaRep(resized, scale_factor);
  }

 private:
  const ImageSkia source_;
  const Size target_dip_size_;

  DISALLOW_COPY_AND_ASSIGN(ResizeSource);
};

// DropShadowSource generates image reps with drop shadow for image reps in
// |source| that represent requested scale factors.
class DropShadowSource : public ImageSkiaSource {
 public:
  DropShadowSource(const ImageSkia& source,
                   const ShadowValues& shadows_in_dip)
      : source_(source),
        shaodws_in_dip_(shadows_in_dip) {
  }
  virtual ~DropShadowSource() {}

  // gfx::ImageSkiaSource overrides:
  virtual ImageSkiaRep GetImageForScale(ui::ScaleFactor scale_factor) OVERRIDE {
    const ImageSkiaRep& image_rep = source_.GetRepresentation(scale_factor);

    const float scale = image_rep.GetScale();
    ShadowValues shaodws_in_pixel;
    for (size_t i = 0; i < shaodws_in_dip_.size(); ++i)
      shaodws_in_pixel.push_back(shaodws_in_dip_[i].Scale(scale));

    const SkBitmap shaodw_bitmap = SkBitmapOperations::CreateDropShadow(
        image_rep.sk_bitmap(),
        shaodws_in_pixel);
    return ImageSkiaRep(shaodw_bitmap, image_rep.scale_factor());
  }

 private:
  const ImageSkia source_;
  const ShadowValues shaodws_in_dip_;

  DISALLOW_COPY_AND_ASSIGN(DropShadowSource);
};

}  // namespace;

// static
ImageSkia ImageSkiaOperations::CreateBlendedImage(const ImageSkia& first,
                                                  const ImageSkia& second,
                                                  double alpha) {
  return ImageSkia(new BlendingImageSource(first, second, alpha), first.size());
}

// static
ImageSkia ImageSkiaOperations::CreateMaskedImage(const ImageSkia& rgb,
                                                 const ImageSkia& alpha) {
  return ImageSkia(new MaskedImageSource(rgb, alpha), rgb.size());
}

// static
ImageSkia ImageSkiaOperations::CreateTiledImage(const ImageSkia& source,
                                                int src_x, int src_y,
                                                int dst_w, int dst_h) {
  return ImageSkia(new TiledImageSource(source, src_x, src_y, dst_w, dst_h),
                   gfx::Size(dst_w, dst_h));
}

// static
ImageSkia ImageSkiaOperations::CreateResizedImage(const ImageSkia& source,
                                                  const Size& target_dip_size) {
  return ImageSkia(new ResizeSource(source, target_dip_size), target_dip_size);
}

// static
ImageSkia ImageSkiaOperations::CreateImageWithDropShadow(
    const ImageSkia& source,
    const ShadowValues& shadows) {
  const gfx::Insets shadow_padding = -gfx::ShadowValue::GetMargin(shadows);
  gfx::Size shadow_image_size = source.size();
  shadow_image_size.Enlarge(shadow_padding.width(),
                            shadow_padding.height());
  return ImageSkia(new DropShadowSource(source, shadows), shadow_image_size);
}

}  // namespace gfx
