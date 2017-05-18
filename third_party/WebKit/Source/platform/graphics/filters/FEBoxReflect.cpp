// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/filters/FEBoxReflect.h"

#include "platform/geometry/FloatRect.h"
#include "platform/graphics/filters/SkiaImageFilterBuilder.h"
#include "platform/wtf/Assertions.h"

namespace blink {

FEBoxReflect::FEBoxReflect(Filter* filter, const BoxReflection& reflection)
    : FilterEffect(filter), reflection_(reflection) {}

FEBoxReflect::~FEBoxReflect() {}

FloatRect FEBoxReflect::MapEffect(const FloatRect& rect) const {
  return reflection_.MapRect(rect);
}

TextStream& FEBoxReflect::ExternalRepresentation(TextStream& ts,
                                                 int indent) const {
  // Only called for SVG layout tree printing.
  NOTREACHED();
  return ts;
}

sk_sp<SkImageFilter> FEBoxReflect::CreateImageFilter() {
  return SkiaImageFilterBuilder::BuildBoxReflectFilter(
      reflection_, SkiaImageFilterBuilder::Build(
                       InputEffect(0), OperatingInterpolationSpace()));
}

}  // namespace blink
