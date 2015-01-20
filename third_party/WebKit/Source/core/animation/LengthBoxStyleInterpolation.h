// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LengthBoxStyleInterpolation_h
#define LengthBoxStyleInterpolation_h

#include "core/animation/LengthStyleInterpolation.h"
#include "core/css/CSSBorderImageSliceValue.h"

namespace blink {

class LengthBoxStyleInterpolation : public StyleInterpolation {
public:
    static PassRefPtrWillBeRawPtr<LengthBoxStyleInterpolation> create(const CSSValue& start, const CSSValue& end, CSSPropertyID id)
    {
        return adoptRefWillBeNoop(new LengthBoxStyleInterpolation(lengthBoxtoInterpolableValue(start), lengthBoxtoInterpolableValue(end), id, false));
    }

    static PassRefPtrWillBeRawPtr<LengthBoxStyleInterpolation> createFromBorderImageSlice(CSSValue& start, CSSValue& end, CSSPropertyID id)
    {
        return adoptRefWillBeNoop(new LengthBoxStyleInterpolation(borderImageSlicetoInterpolableValue(start), borderImageSlicetoInterpolableValue(end), id, toCSSBorderImageSliceValue(start).m_fill));
    }

    static bool canCreateFrom(const CSSValue&);

    static bool matchingFill(CSSValue&, CSSValue&);
    static bool canCreateFromBorderImageSlice(CSSValue&);

    virtual void apply(StyleResolverState&) const override;
    virtual void trace(Visitor*) override;

private:
    LengthBoxStyleInterpolation(PassOwnPtrWillBeRawPtr<InterpolableValue> start, PassOwnPtrWillBeRawPtr<InterpolableValue> end, CSSPropertyID id, bool fill)
        : StyleInterpolation(start, end, id)
        , m_fill(fill)
    { }

    static PassOwnPtrWillBeRawPtr<InterpolableValue> lengthBoxtoInterpolableValue(const CSSValue&);
    static PassOwnPtrWillBeRawPtr<InterpolableValue> borderImageSlicetoInterpolableValue(CSSValue&);
    static PassRefPtrWillBeRawPtr<CSSValue> interpolableValueToLengthBox(InterpolableValue*, InterpolationRange = RangeAll);
    static PassRefPtrWillBeRawPtr<CSSValue> interpolableValueToBorderImageSlice(InterpolableValue*, bool);

    bool m_fill;

    friend class AnimationLengthBoxStyleInterpolationTest;
};

} // namespace blink

#endif // LengthBoxStyleInterpolation_h
