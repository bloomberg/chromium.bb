// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LengthStyleInterpolation_h
#define LengthStyleInterpolation_h

#include "core/CoreExport.h"
#include "core/animation/StyleInterpolation.h"
#include "core/css/CSSPrimitiveValue.h"

namespace blink {

class ComputedStyle;
class Length;

class CORE_EXPORT LengthStyleInterpolation : public StyleInterpolation {
public:
    typedef void NonInterpolableType;

    static PassRefPtr<LengthStyleInterpolation> create(const CSSValue& start, const CSSValue& end, CSSPropertyID id, InterpolationRange range)
    {
        return adoptRef(new LengthStyleInterpolation(toInterpolableValue(start, id), toInterpolableValue(end, id), id, range));
    }

    static bool canCreateFrom(const CSSValue&, CSSPropertyID = CSSPropertyInvalid);

    void apply(StyleResolverState&) const override;

    static PassOwnPtr<InterpolableValue> toInterpolableValue(const CSSValue&, CSSPropertyID = CSSPropertyInvalid);
    static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> fromInterpolableValue(const InterpolableValue&, InterpolationRange);
    static void applyInterpolableValue(CSSPropertyID, const InterpolableValue&, InterpolationRange, StyleResolverState&);

private:
    LengthStyleInterpolation(PassOwnPtr<InterpolableValue> start, PassOwnPtr<InterpolableValue> end, CSSPropertyID id, InterpolationRange range)
        : StyleInterpolation(std::move(start), std::move(end), id)
        , m_range(range)
    {
    }

    InterpolationRange m_range;
};

} // namespace blink

#endif
