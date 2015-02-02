// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGLengthStyleInterpolation_h
#define SVGLengthStyleInterpolation_h

#include "core/animation/StyleInterpolation.h"
#include "core/css/CSSPrimitiveValue.h"
#include "platform/Length.h"

namespace blink {

class SVGLengthStyleInterpolation : public StyleInterpolation {
public:
    static PassRefPtrWillBeRawPtr<SVGLengthStyleInterpolation> maybeCreate(const CSSValue& start, const CSSValue& end, CSSPropertyID, InterpolationRange);

    virtual void apply(StyleResolverState&) const override;

    virtual void trace(Visitor* visitor) override
    {
        StyleInterpolation::trace(visitor);
    }

private:
    SVGLengthStyleInterpolation(const CSSPrimitiveValue& start, const CSSPrimitiveValue& end, CSSPropertyID, CSSPrimitiveValue::UnitType, InterpolationRange);

    static bool canCreateFrom(const CSSValue&);
    static PassOwnPtrWillBeRawPtr<InterpolableValue> lengthToInterpolableValue(const CSSPrimitiveValue&);
    static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> interpolableValueToLength(const InterpolableValue&, CSSPrimitiveValue::UnitType, InterpolationRange);

    CSSPrimitiveValue::UnitType m_type;
    InterpolationRange m_range;

    friend class AnimationSVGLengthStyleInterpolationTest;
};

}

#endif // SVGLengthStyleInterpolation_h
