// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LengthBoxStyleInterpolation_h
#define LengthBoxStyleInterpolation_h

#include "core/animation/LengthStyleInterpolation.h"
#include "core/css/CSSBorderImageSliceValue.h"
#include "core/css/Rect.h"

namespace blink {

class LengthBoxStyleInterpolation : public StyleInterpolation {
public:
    static PassRefPtrWillBeRawPtr<LengthBoxStyleInterpolation> maybeCreateFrom(CSSValue&, CSSValue&, CSSPropertyID);

    static bool usesDefaultInterpolation(const CSSValue&, const CSSValue&);

    virtual void apply(StyleResolverState&) const override;
    virtual void trace(Visitor*) override;

private:
    LengthBoxStyleInterpolation(PassOwnPtrWillBeRawPtr<InterpolableValue> startInterpolation, PassOwnPtrWillBeRawPtr<InterpolableValue> endInterpolation, CSSPropertyID id, bool fill, CSSValue* startCSS, CSSValue* endCSS)
        : StyleInterpolation(startInterpolation, endInterpolation, id)
        , m_fill(fill)
        , m_startCSSValue(startCSS)
        , m_endCSSValue(endCSS)
    { }

    static PassOwnPtrWillBeRawPtr<InterpolableValue> lengthBoxtoInterpolableValue(const CSSValue&, const CSSValue&, bool);
    static PassOwnPtrWillBeRawPtr<InterpolableValue> borderImageSlicetoInterpolableValue(const CSSValue&);
    static PassRefPtrWillBeRawPtr<CSSValue> interpolableValueToLengthBox(InterpolableValue*, const CSSValue&, const CSSValue&);
    static PassRefPtrWillBeRawPtr<CSSValue> interpolableValueToBorderImageSlice(InterpolableValue*, bool);

    bool m_fill;
    RefPtrWillBeMember<CSSValue> m_startCSSValue;
    RefPtrWillBeMember<CSSValue> m_endCSSValue;

    friend class AnimationLengthBoxStyleInterpolationTest;
};

} // namespace blink

#endif // LengthBoxStyleInterpolation_h
