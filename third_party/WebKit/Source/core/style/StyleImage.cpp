// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/style/StyleImage.h"

#include "core/svg/graphics/SVGImage.h"
#include "core/svg/graphics/SVGImageForContainer.h"
#include "platform/InstanceCounters.h"
#include "platform/geometry/LayoutSize.h"

namespace blink {

StyleImage::~StyleImage() {
  if (is_ua_css_resource_)
    InstanceCounters::DecrementCounter(InstanceCounters::kUACSSResourceCounter);
}

LayoutSize StyleImage::ApplyZoom(const LayoutSize& size,
                                 float multiplier) const {
  if (multiplier == 1.0f || ImageHasRelativeSize())
    return size;

  LayoutUnit width(size.Width() * multiplier);
  LayoutUnit height(size.Height() * multiplier);

  // Don't let images that have a width/height >= 1 shrink below 1 when zoomed.
  if (size.Width() > LayoutUnit())
    width = std::max(LayoutUnit(1), width);

  if (size.Height() > LayoutUnit())
    height = std::max(LayoutUnit(1), height);

  return LayoutSize(width, height);
}

LayoutSize StyleImage::ImageSizeForSVGImage(
    SVGImage* svg_image,
    float multiplier,
    const LayoutSize& default_object_size) const {
  FloatSize unzoomed_default_object_size(default_object_size);
  unzoomed_default_object_size.Scale(1 / multiplier);
  LayoutSize image_size(RoundedIntSize(
      svg_image->ConcreteObjectSize(unzoomed_default_object_size)));
  return ApplyZoom(image_size, multiplier);
}

void StyleImage::FlagAsUserAgentResource() {
  if (is_ua_css_resource_)
    return;

  InstanceCounters::IncrementCounter(InstanceCounters::kUACSSResourceCounter);
  is_ua_css_resource_ = true;
}

}  // namespace blink
