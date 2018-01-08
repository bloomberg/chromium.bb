// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/style/StyleImage.h"

#include "core/svg/graphics/SVGImage.h"
#include "core/svg/graphics/SVGImageForContainer.h"
#include "platform/geometry/LayoutSize.h"

namespace blink {

FloatSize StyleImage::ApplyZoom(const FloatSize& size, float multiplier) const {
  if (multiplier == 1.0f || ImageHasRelativeSize())
    return size;

  float width = size.Width() * multiplier;
  float height = size.Height() * multiplier;

  // Don't let images that have a width/height >= 1 shrink below 1 when zoomed.
  if (size.Width() > 0)
    width = std::max(1.0f, width);

  if (size.Height() > 0)
    height = std::max(1.0f, height);

  return FloatSize(width, height);
}

FloatSize StyleImage::ImageSizeForSVGImage(
    SVGImage* svg_image,
    float multiplier,
    const LayoutSize& default_object_size) const {
  FloatSize unzoomed_default_object_size(default_object_size);
  unzoomed_default_object_size.Scale(1 / multiplier);
  // FIXME(schenney): Remove this rounding hack once background image
  // geometry is converted to handle rounding downstream.
  return FloatSize(RoundedIntSize(
      ApplyZoom(svg_image->ConcreteObjectSize(unzoomed_default_object_size),
                multiplier)));
}

}  // namespace blink
