// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NumberOptionalNumberSVGInterpolation_h
#define NumberOptionalNumberSVGInterpolation_h

#include "core/animation/SVGInterpolation.h"
#include "core/svg/SVGNumber.h"
#include "core/svg/SVGNumberOptionalNumber.h"

namespace blink {

class SVGNumberOptionalNumber;

class NumberOptionalNumberSVGInterpolation : public SVGInterpolation {
public:
    static PassRefPtr<NumberOptionalNumberSVGInterpolation> create(SVGPropertyBase* start, SVGPropertyBase* end, PassRefPtrWillBeRawPtr<SVGAnimatedPropertyBase> attribute)
    {
        return adoptRef(new NumberOptionalNumberSVGInterpolation(toInterpolableValue(start), toInterpolableValue(end), attribute));
    }

    PassRefPtrWillBeRawPtr<SVGPropertyBase> interpolatedValue(SVGElement&) const final
    {
        return fromInterpolableValue(*m_cachedValue);
    }

private:
    NumberOptionalNumberSVGInterpolation(PassOwnPtr<InterpolableValue> start, PassOwnPtr<InterpolableValue> end, PassRefPtrWillBeRawPtr<SVGAnimatedPropertyBase> attribute)
        : SVGInterpolation(start, end, attribute)
    {
    }

    static PassOwnPtr<InterpolableValue> toInterpolableValue(SVGPropertyBase* value)
    {
        RefPtrWillBeRawPtr<SVGNumberOptionalNumber> numberOptionalNumber = toSVGNumberOptionalNumber(value);
        OwnPtr<InterpolableList> result = InterpolableList::create(2);
        result->set(0, InterpolableNumber::create(numberOptionalNumber->firstNumber()->value()));
        result->set(1, InterpolableNumber::create(numberOptionalNumber->secondNumber()->value()));
        return result.release();
    }

    static PassRefPtrWillBeRawPtr<SVGNumberOptionalNumber> fromInterpolableValue(const InterpolableValue& value)
    {
        const InterpolableList& list = toInterpolableList(value);
        return SVGNumberOptionalNumber::create(
            SVGNumber::create(toInterpolableNumber(list.get(0))->value()),
            SVGNumber::create(toInterpolableNumber(list.get(1))->value()));
    }
};

}

#endif // NumberOptionalNumberSVGInterpolation_h
