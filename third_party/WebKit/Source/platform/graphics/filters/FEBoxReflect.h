// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FEBoxReflect_h
#define FEBoxReflect_h

#include "platform/PlatformExport.h"
#include "platform/graphics/GraphicsTypes.h"
#include "platform/graphics/filters/FilterEffect.h"

namespace blink {

// Used to implement the -webkit-box-reflect property as a filter.
class PLATFORM_EXPORT FEBoxReflect final : public FilterEffect {
public:
    static FEBoxReflect* create(Filter*, ReflectionDirection, float offset);

    // FilterEffect implementation
    FloatRect mapRect(const FloatRect&, bool forward = true) const final;
    TextStream& externalRepresentation(TextStream&, int indentation) const final;
    PassRefPtr<SkImageFilter> createImageFilter(SkiaImageFilterBuilder&) final;

private:
    FEBoxReflect(Filter*, ReflectionDirection, float offset);
    ~FEBoxReflect() final;

    ReflectionDirection m_reflectionDirection;
    float m_offset;
};

} // namespace blink

#endif // FEBoxReflect_h
