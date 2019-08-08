// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_FILTER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_FILTER_H_

#include "cc/paint/paint_flags.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_settings.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/lab_color_space.h"
#include "third_party/skia/include/core/SkRefCnt.h"

class SkColorFilter;

namespace blink {

class DarkModeFilter {
 public:
  DarkModeFilter();

  const DarkModeSettings& settings() const { return settings_; }
  void UpdateSettings(const DarkModeSettings& new_settings);

  Color ApplyIfNeeded(const Color& color);

  // |image| and |flags| must not be null.
  void ApplyToImageFlagsIfNeeded(const FloatRect& src_rect,
                                 Image* image,
                                 cc::PaintFlags* flags);

  // |flags| must not be null.
  base::Optional<cc::PaintFlags> ApplyToFlagsIfNeeded(
      const cc::PaintFlags& flags);

  Color InvertColor(const Color& color) const;

  bool ShouldInvertTextColor(const Color& color) const;

 private:
  DarkModeSettings settings_;

  sk_sp<SkColorFilter> default_filter_;
  sk_sp<SkColorFilter> image_filter_;
  base::Optional<LabColorSpace::RGBLABTransformer> transformer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_FILTER_H_
