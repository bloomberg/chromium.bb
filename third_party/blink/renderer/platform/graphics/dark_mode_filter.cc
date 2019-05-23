// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/dark_mode_filter.h"

#include "base/optional.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_settings.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/effects/SkColorMatrix.h"
#include "third_party/skia/include/effects/SkHighContrastFilter.h"
#include "third_party/skia/include/effects/SkTableColorFilter.h"

namespace blink {
namespace {

bool ShouldApplyToImage(const DarkModeSettings& settings,
                        const FloatRect& src_rect,
                        Image* image) {
  switch (settings.image_policy) {
    case DarkModeImagePolicy::kFilterSmart:
      return image->ShouldApplyDarkModeFilter(src_rect);
    case DarkModeImagePolicy::kFilterAll:
      return true;
    default:
      return false;
  }
}

// |grayscale_percent| should be between 0.0 and 1.0.
sk_sp<SkColorFilter> MakeGrayscaleFilter(float grayscale_percent) {
  DCHECK_GE(grayscale_percent, 0.0f);
  DCHECK_LE(grayscale_percent, 1.0f);

  SkColorMatrix grayscale_matrix;
  grayscale_matrix.setSaturation(1.0f - grayscale_percent);
  return SkColorFilters::Matrix(grayscale_matrix);
}

}  // namespace

DarkModeFilter::DarkModeFilter()
    : default_filter_(nullptr), image_filter_(nullptr) {
  settings_.mode = DarkMode::kOff;
  settings_.image_policy = DarkModeImagePolicy::kFilterNone;
}

void DarkModeFilter::UpdateSettings(const DarkModeSettings& new_settings) {
  settings_ = new_settings;

  SkHighContrastConfig config;
  transformer_ = base::nullopt;
  switch (settings_.mode) {
    case DarkMode::kOff:
      default_filter_.reset(nullptr);
      image_filter_.reset(nullptr);
      return;
    case DarkMode::kSimpleInvertForTesting: {
      uint8_t identity[256], invert[256];
      for (int i = 0; i < 256; ++i) {
        identity[i] = i;
        invert[i] = 255 - i;
      }
      default_filter_ =
          SkTableColorFilter::MakeARGB(identity, invert, invert, invert);
      image_filter_.reset(nullptr);
      return;
    }
    case DarkMode::kInvertBrightness:
      config.fInvertStyle =
          SkHighContrastConfig::InvertStyle::kInvertBrightness;
      break;
    case DarkMode::kInvertLightness:
      config.fInvertStyle = SkHighContrastConfig::InvertStyle::kInvertLightness;
      break;
    case DarkMode::kInvertLightnessLAB:
      transformer_ = LabColorSpace::RGBLABTransformer();
      config.fInvertStyle = SkHighContrastConfig::InvertStyle::kInvertLightness;
      break;
  }

  config.fGrayscale = settings_.grayscale;
  config.fContrast = settings_.contrast;
  default_filter_ = SkHighContrastFilter::Make(config);

  if (settings_.image_grayscale_percent > 0.0f)
    image_filter_ = MakeGrayscaleFilter(settings_.image_grayscale_percent);
  else
    image_filter_.reset(nullptr);
}

Color DarkModeFilter::ApplyIfNeeded(const Color& color) {
  if (!default_filter_)
    return color;
  if (!transformer_)
    return Color(default_filter_->filterColor(color.Rgb()));

  return InvertColor(color);
}

// TODO(gilmanmh): Investigate making |image| a const reference. This code
// relies on Image::ShouldApplyDarkModeFilter(), which is not const. If it
// could be made const, then |image| could also be const.
void DarkModeFilter::ApplyToImageFlagsIfNeeded(const FloatRect& src_rect,
                                               Image* image,
                                               cc::PaintFlags* flags) {
  sk_sp<SkColorFilter> filter = image_filter_;
  if (!filter)
    filter = default_filter_;

  if (!filter || !ShouldApplyToImage(settings(), src_rect, image))
    return;
  flags->setColorFilter(std::move(filter));
}

base::Optional<cc::PaintFlags> DarkModeFilter::ApplyToFlagsIfNeeded(
    const cc::PaintFlags& flags) {
  if (!default_filter_)
    return base::nullopt;

  cc::PaintFlags dark_mode_flags = flags;
  if (flags.HasShader()) {
    dark_mode_flags.setColorFilter(default_filter_);
  } else {
    auto invertedColor = ApplyIfNeeded(flags.getColor());
    dark_mode_flags.setColor(
        SkColorSetARGB(invertedColor.Alpha(), invertedColor.Red(),
                       invertedColor.Green(), invertedColor.Blue()));
  }

  return base::make_optional<cc::PaintFlags>(std::move(dark_mode_flags));
}

Color DarkModeFilter::InvertColor(const Color& color) const {
  blink::FloatPoint3D rgb = {color.Red() / 255.0f, color.Green() / 255.0f,
                             color.Blue() / 255.0f};
  blink::FloatPoint3D lab = transformer_->sRGBToLab(rgb);
  float invertedL = std::min(110.0f - lab.X(), 100.0f);
  lab.SetX(invertedL);
  rgb = transformer_->LabToSRGB(lab);

  return Color(static_cast<unsigned int>(rgb.X() * 255 + 0.5),
               static_cast<unsigned int>(rgb.Y() * 255 + 0.5),
               static_cast<unsigned int>(rgb.Z() * 255 + 0.5), color.Alpha());
}

bool DarkModeFilter::ShouldInvertTextColor(const Color& color) const {
  if (settings_.text_policy == DarkModeTextPolicy::kInvertAll)
    return true;

  // Throw an error in debug mode if new values are added to the enum without
  // updating this method.
  DCHECK_EQ(settings_.text_policy, DarkModeTextPolicy::kInvertDarkOnly);
  if (color == Color::kWhite) {
    return false;
  }
  return true;
}

}  // namespace blink
