// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LengthStyleInterpolation_h
#define LengthStyleInterpolation_h

#include "core/animation/StyleInterpolation.h"
#include "core/css/CSSPrimitiveValue.h"
#include "platform/Length.h"

namespace blink {

class LengthStyleInterpolation : public StyleInterpolation {
public:

    typedef void NonInterpolableType;

    static PassRefPtrWillBeRawPtr<LengthStyleInterpolation> create(const CSSValue& start, const CSSValue& end, CSSPropertyID id, InterpolationRange range)
    {
        return adoptRefWillBeNoop(new LengthStyleInterpolation(toInterpolableValue(start), toInterpolableValue(end), id, range));
    }

    static bool canCreateFrom(const CSSValue&);

    virtual void apply(StyleResolverState&) const override;

    virtual void trace(Visitor*) override;

    static PassOwnPtrWillBeRawPtr<InterpolableValue> toInterpolableValue(const CSSValue&);
    static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> fromInterpolableValue(const InterpolableValue&, InterpolationRange);

private:
    LengthStyleInterpolation(PassOwnPtrWillBeRawPtr<InterpolableValue> start, PassOwnPtrWillBeRawPtr<InterpolableValue> end, CSSPropertyID id,  InterpolationRange range)
        : StyleInterpolation(start, end, id)
        , m_range(range)
        { }

    InterpolationRange m_range;
};

}

#endif
