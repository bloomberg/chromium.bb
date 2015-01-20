// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LengthPoint3DStyleInterpolation_h
#define LengthPoint3DStyleInterpolation_h

#include "core/animation/LengthStyleInterpolation.h"
#include "core/css/CSSValueList.h"

namespace blink {

class LengthPoint3DStyleInterpolation : public StyleInterpolation {
public:
    static PassRefPtrWillBeRawPtr<LengthPoint3DStyleInterpolation> create(const CSSValue& start, const CSSValue& end, CSSPropertyID id)
    {
        return adoptRefWillBeNoop(new LengthPoint3DStyleInterpolation(lengthPoint3DtoInterpolableValue(start), lengthPoint3DtoInterpolableValue(end), id));
    }

    static bool canCreateFrom(const CSSValue&);

    virtual void apply(StyleResolverState&) const override;
    virtual void trace(Visitor*) override;

private:
    LengthPoint3DStyleInterpolation(PassOwnPtrWillBeRawPtr<InterpolableValue> start, PassOwnPtrWillBeRawPtr<InterpolableValue> end, CSSPropertyID id)
        : StyleInterpolation(start, end, id)
    { }

    static PassOwnPtrWillBeRawPtr<InterpolableValue> lengthPoint3DtoInterpolableValue(const CSSValue&);
    static PassRefPtrWillBeRawPtr<CSSValue> interpolableValueToLengthPoint3D(InterpolableValue*, InterpolationRange = RangeAll);

    friend class AnimationLengthPoint3DStyleInterpolationTest;
};

} // namespace blink

#endif // LengthPoint3DStyleInterpolation_h
