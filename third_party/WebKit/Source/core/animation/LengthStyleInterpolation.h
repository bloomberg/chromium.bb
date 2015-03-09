// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LengthStyleInterpolation_h
#define LengthStyleInterpolation_h

#include "core/animation/StyleInterpolation.h"
#include "core/css/CSSPrimitiveValue.h"

namespace blink {

class LayoutStyle;
class Length;

class LengthStyleInterpolation : public StyleInterpolation {
public:
    typedef void (LayoutStyle::*LengthSetter)(const Length&);
    typedef void NonInterpolableType;

    static PassRefPtrWillBeRawPtr<LengthStyleInterpolation> create(const CSSValue& start, const CSSValue& end, CSSPropertyID id, InterpolationRange range)
    {
        return adoptRefWillBeNoop(new LengthStyleInterpolation(toInterpolableValue(start), toInterpolableValue(end), id, range));
    }

    static bool canCreateFrom(const CSSValue&);

    virtual void apply(StyleResolverState&) const override;

    DECLARE_VIRTUAL_TRACE();

    static PassOwnPtrWillBeRawPtr<InterpolableValue> toInterpolableValue(const CSSValue&);
    static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> fromInterpolableValue(const InterpolableValue&, InterpolationRange);

private:
    LengthStyleInterpolation(PassOwnPtrWillBeRawPtr<InterpolableValue> start, PassOwnPtrWillBeRawPtr<InterpolableValue> end, CSSPropertyID id, InterpolationRange range)
        : StyleInterpolation(start, end, id)
        , m_range(range)
        , m_lengthSetter(nullptr)
    {
        if (isPixelsOrPercentOnly(*m_start) && isPixelsOrPercentOnly(*m_end))
            m_lengthSetter = lengthSetterForProperty(id);
    }

    static bool isPixelsOrPercentOnly(const InterpolableValue&);
    static LengthSetter lengthSetterForProperty(CSSPropertyID);

    InterpolationRange m_range;
    LengthSetter m_lengthSetter;
};

}

#endif
