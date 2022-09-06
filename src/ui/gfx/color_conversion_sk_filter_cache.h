// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COLOR_CONVERSION_SK_FILTER_CACHE_H_
#define UI_GFX_COLOR_CONVERSION_SK_FILTER_CACHE_H_

#include "base/containers/flat_map.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/color_space_export.h"
#include "ui/gfx/gfx_export.h"

class GrDirectContext;
class SkImage;
class SkColorFilter;
class SkRuntimeEffect;

namespace gfx {

class COLOR_SPACE_EXPORT ColorConversionSkFilterCache {
 public:
  ColorConversionSkFilterCache();
  ColorConversionSkFilterCache(const ColorConversionSkFilterCache&) = delete;
  ColorConversionSkFilterCache& operator=(const ColorConversionSkFilterCache&) =
      delete;
  ~ColorConversionSkFilterCache();

  // Retrieve an SkColorFilter to transform `src` to `dst`. The filter also
  // applies the offset `resource_offset` and then scales by
  // `resource_multiplier`.
  // TODO(https://crbug.com/1286076): Apply tone mapping using
  // `sdr_max_luminance_nits` and `dst_max_luminance_relative`.
  sk_sp<SkColorFilter> Get(const gfx::ColorSpace& src,
                           const gfx::ColorSpace& dst,
                           float resource_offset,
                           float resource_multiplier,
                           float sdr_max_luminance_nits,
                           float dst_max_luminance_relative);

  // Convert `image` to be in `target_color_space`, performing tone mapping as
  // needed (using `sdr_max_luminance_nits` and `dst_max_luminance_relative`).
  // If `image` is GPU backed then `context` should be its GrDirectContext,
  // otherwise, `context` should be nullptr. The resulting image will not have
  // mipmaps.
  // If the feature ImageToneMapping is disabled, then this function is
  // equivalent to calling `image->makeColorSpace(target_color_space, context)`,
  // and no tone mapping is performed.
  sk_sp<SkImage> ConvertImage(sk_sp<SkImage> image,
                              sk_sp<SkColorSpace> target_color_space,
                              float sdr_max_luminance_nits,
                              float dst_max_luminance_relative,
                              GrDirectContext* context);

 public:
  struct Key {
    Key(const gfx::ColorSpace& src,
        const gfx::ColorSpace& dst,
        float sdr_max_luminance_nits,
        float dst_max_luminance_relative);

    gfx::ColorSpace src;
    gfx::ColorSpace dst;
    float sdr_max_luminance_nits = 0.f;
    float dst_max_luminance_relative = 0.f;

    bool operator==(const Key& other) const;
    bool operator!=(const Key& other) const;
    bool operator<(const Key& other) const;
  };
  static Key KeyForParams(const gfx::ColorSpace& src,
                          const gfx::ColorSpace& dst,
                          float resource_offset,
                          float resource_multiplier,
                          float sdr_max_luminance_nits,
                          float dst_max_luminance_relative);

  base::flat_map<Key, sk_sp<SkRuntimeEffect>> cache_;
};

}  // namespace gfx

#endif  // UI_GFX_COLOR_CONVERSION_SK_FILTER_CACHE_H_
