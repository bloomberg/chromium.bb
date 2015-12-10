// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGInterpolation_h
#define SVGInterpolation_h

#include "core/animation/Interpolation.h"
#include "core/animation/PropertyHandle.h"
#include "core/svg/properties/SVGAnimatedProperty.h"

namespace blink {

class SVGInterpolation : public Interpolation {
public:
    bool isSVGInterpolation() const final { return true; }

    SVGAnimatedPropertyBase* attribute() const { return m_attribute.get(); }

    const QualifiedName& attributeName() const { return m_attribute->attributeName(); }

    PropertyHandle property() const final
    {
        return PropertyHandle(attributeName());
    }

    void apply(SVGElement& targetElement) const
    {
        targetElement.setWebAnimatedAttribute(attributeName(), interpolatedValue(targetElement));
    }

    virtual PassRefPtrWillBeRawPtr<SVGPropertyBase> interpolatedValue(SVGElement&) const = 0;

protected:
    SVGInterpolation(PassOwnPtr<InterpolableValue> start, PassOwnPtr<InterpolableValue> end, PassRefPtrWillBeRawPtr<SVGAnimatedPropertyBase> attribute)
        : Interpolation(start, end)
        , m_attribute(attribute)
    {
    }

    RefPtrWillBePersistent<SVGAnimatedPropertyBase> m_attribute;
};

DEFINE_TYPE_CASTS(SVGInterpolation, Interpolation, value, value->isSVGInterpolation(), value.isSVGInterpolation());

}

#endif // SVGInterpolation_h
