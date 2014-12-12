// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LengthBoxStyleInterpolation_h
#define LengthBoxStyleInterpolation_h

#include "core/animation/LengthStyleInterpolation.h"

namespace blink {

class LengthBoxStyleInterpolation : public StyleInterpolation {
public:
    static PassRefPtrWillBeRawPtr<LengthBoxStyleInterpolation> create(const CSSValue& start, const CSSValue& end, CSSPropertyID id)
    {
        return adoptRefWillBeNoop(new LengthBoxStyleInterpolation(lengthBoxtoInterpolableValue(start), lengthBoxtoInterpolableValue(end), id));
    }

    static bool canCreateFrom(const CSSValue&);

    virtual void apply(StyleResolverState&) const override;
    virtual void trace(Visitor*) override;

private:
    LengthBoxStyleInterpolation(PassOwnPtrWillBeRawPtr<InterpolableValue> start, PassOwnPtrWillBeRawPtr<InterpolableValue> end, CSSPropertyID id)
        : StyleInterpolation(start, end, id)
    { }

    static PassOwnPtrWillBeRawPtr<InterpolableValue> lengthBoxtoInterpolableValue(const CSSValue&);
    static PassRefPtrWillBeRawPtr<CSSValue> interpolableValueToLengthBox(InterpolableValue*);

    friend class AnimationLengthBoxStyleInterpolationTest;
};

} // namespace blink

#endif // LengthBoxStyleInterpolation_h


