/*
 * Copyright (c) 2008, Google Inc. All rights reserved.
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/graphics/InterpolationSpace.h"

#include <algorithm>
#include "platform/graphics/skia/SkiaUtils.h"
#include "platform/wtf/MathExtras.h"
#include "third_party/skia/include/effects/SkTableColorFilter.h"

namespace blink {

namespace InterpolationSpaceUtilities {

static const uint8_t* GetLinearRgbLUT() {
  static uint8_t linear_rgb_lut[256];
  static bool initialized;
  if (!initialized) {
    for (unsigned i = 0; i < 256; i++) {
      float color = i / 255.0f;
      color = (color <= 0.04045f ? color / 12.92f
                                 : pow((color + 0.055f) / 1.055f, 2.4f));
      color = std::max(0.0f, color);
      color = std::min(1.0f, color);
      linear_rgb_lut[i] = static_cast<uint8_t>(round(color * 255));
    }
    initialized = true;
  }
  return linear_rgb_lut;
}

static const uint8_t* GetDeviceRgbLUT() {
  static uint8_t device_rgb_lut[256];
  static bool initialized;
  if (!initialized) {
    for (unsigned i = 0; i < 256; i++) {
      float color = i / 255.0f;
      color = (powf(color, 1.0f / 2.4f) * 1.055f) - 0.055f;
      color = std::max(0.0f, color);
      color = std::min(1.0f, color);
      device_rgb_lut[i] = static_cast<uint8_t>(round(color * 255));
    }
    initialized = true;
  }
  return device_rgb_lut;
}

const uint8_t* GetConversionLUT(InterpolationSpace dst_interpolation_space,
                                InterpolationSpace src_interpolation_space) {
  // Identity.
  if (src_interpolation_space == dst_interpolation_space)
    return 0;

  // Only sRGB/DeviceRGB <-> linearRGB are supported at the moment.
  if ((src_interpolation_space != kInterpolationSpaceLinear &&
       src_interpolation_space != kInterpolationSpaceSRGB) ||
      (dst_interpolation_space != kInterpolationSpaceLinear &&
       dst_interpolation_space != kInterpolationSpaceSRGB))
    return 0;

  if (dst_interpolation_space == kInterpolationSpaceLinear)
    return GetLinearRgbLUT();
  if (dst_interpolation_space == kInterpolationSpaceSRGB)
    return GetDeviceRgbLUT();

  NOTREACHED();
  return 0;
}

Color ConvertColor(const Color& src_color,
                   InterpolationSpace dst_interpolation_space,
                   InterpolationSpace src_interpolation_space) {
  const uint8_t* lookup_table =
      GetConversionLUT(dst_interpolation_space, src_interpolation_space);
  if (!lookup_table)
    return src_color;

  return Color(lookup_table[src_color.Red()], lookup_table[src_color.Green()],
               lookup_table[src_color.Blue()], src_color.Alpha());
}

sk_sp<SkColorFilter> CreateInterpolationSpaceFilter(
    InterpolationSpace src_interpolation_space,
    InterpolationSpace dst_interpolation_space) {
  const uint8_t* lookup_table =
      GetConversionLUT(dst_interpolation_space, src_interpolation_space);
  if (!lookup_table)
    return nullptr;

  return SkTableColorFilter::MakeARGB(0, lookup_table, lookup_table,
                                      lookup_table);
}

}  // namespace InterpolationSpaceUtilities

}  // namespace blink
