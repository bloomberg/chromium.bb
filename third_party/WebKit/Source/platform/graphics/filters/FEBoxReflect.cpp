// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/filters/FEBoxReflect.h"

#include "platform/geometry/FloatRect.h"
#include "platform/graphics/filters/SkiaImageFilterBuilder.h"
#include "wtf/Assertions.h"

namespace blink {

FEBoxReflect* FEBoxReflect::create(Filter* filter, ReflectionDirection direction, float offset)
{
    return new FEBoxReflect(filter, direction, offset);
}

FEBoxReflect::FEBoxReflect(Filter* filter, ReflectionDirection direction, float offset)
    : FilterEffect(filter)
    , m_reflectionDirection(direction)
    , m_offset(offset)
{
}

FEBoxReflect::~FEBoxReflect()
{
}

FloatRect FEBoxReflect::mapRect(const FloatRect& rect, bool forward) const
{
    // Reflection about any line is self-inverse, so this matrix works for both
    // forward and reverse mapping.
    SkMatrix flipMatrix = SkiaImageFilterBuilder().matrixForBoxReflectFilter(
        m_reflectionDirection, m_offset);

    SkRect reflection(rect);
    flipMatrix.mapRect(&reflection);

    FloatRect result = rect;
    result.unite(reflection);
    return result;
}

TextStream& FEBoxReflect::externalRepresentation(TextStream& ts, int indent) const
{
    // Only called for SVG layout tree printing.
    ASSERT_NOT_REACHED();
    return ts;
}

PassRefPtr<SkImageFilter> FEBoxReflect::createImageFilter(SkiaImageFilterBuilder& builder)
{
    RefPtr<SkImageFilter> input(builder.build(inputEffect(0), operatingColorSpace()));
    return builder.buildBoxReflectFilter(m_reflectionDirection, m_offset, nullptr, input.get());
}

} // namespace blink
