// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/filters/PaintFilterEffect.h"

#include "platform/graphics/filters/Filter.h"
#include "platform/text/TextStream.h"
#include "third_party/skia/include/effects/SkPaintImageFilter.h"

namespace blink {

PaintFilterEffect::PaintFilterEffect(Filter* filter, const PaintFlags& flags)
    : FilterEffect(filter), m_flags(flags) {
  setOperatingColorSpace(ColorSpaceDeviceRGB);
}

PaintFilterEffect::~PaintFilterEffect() {}

PaintFilterEffect* PaintFilterEffect::create(Filter* filter,
                                             const PaintFlags& flags) {
  return new PaintFilterEffect(filter, flags);
}

sk_sp<SkImageFilter> PaintFilterEffect::createImageFilter() {
  return SkPaintImageFilter::Make(ToSkPaint(m_flags), nullptr);
}

TextStream& PaintFilterEffect::externalRepresentation(TextStream& ts,
                                                      int indent) const {
  writeIndent(ts, indent);
  ts << "[PaintFilterEffect]\n";
  return ts;
}

}  // namespace blink
