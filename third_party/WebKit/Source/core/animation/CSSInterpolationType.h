// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSInterpolationType_h
#define CSSInterpolationType_h

#include "core/animation/InterpolationEnvironment.h"
#include "core/animation/InterpolationType.h"
#include "core/animation/StringKeyframe.h"
#include "core/css/CSSPropertyMetadata.h"

namespace blink {

class CSSInterpolationType : public InterpolationType {
protected:
    CSSInterpolationType(CSSPropertyID property)
        : InterpolationType(PropertyHandle(property))
    { }

    CSSPropertyID cssProperty() const { return property().cssProperty(); }

    virtual PassOwnPtr<InterpolationValue> maybeConvertSingle(const PropertySpecificKeyframe& keyframe, const InterpolationEnvironment& environment, const UnderlyingValue& underlyingValue, ConversionCheckers& conversionCheckers) const
    {
        const CSSValue* value = toCSSPropertySpecificKeyframe(keyframe).value();
        if (!value)
            return maybeConvertNeutral(underlyingValue, conversionCheckers);
        if (value->isInitialValue() || (value->isUnsetValue() && !CSSPropertyMetadata::isInheritedProperty(cssProperty())))
            return maybeConvertInitial();
        if (value->isInheritedValue() || (value->isUnsetValue() && CSSPropertyMetadata::isInheritedProperty(cssProperty())))
            return maybeConvertInherit(environment.state(), conversionCheckers);
        return maybeConvertValue(*value, environment.state(), conversionCheckers);
    }

    virtual PassOwnPtr<InterpolationValue> maybeConvertNeutral(const UnderlyingValue&, ConversionCheckers&) const { ASSERT_NOT_REACHED(); return nullptr; }
    virtual PassOwnPtr<InterpolationValue> maybeConvertInitial() const { ASSERT_NOT_REACHED(); return nullptr; }
    virtual PassOwnPtr<InterpolationValue> maybeConvertInherit(const StyleResolverState&, ConversionCheckers& conversionCheckers) const { ASSERT_NOT_REACHED(); return nullptr; }
    virtual PassOwnPtr<InterpolationValue> maybeConvertValue(const CSSValue& value, const StyleResolverState&, ConversionCheckers& conversionCheckers) const { ASSERT_NOT_REACHED(); return nullptr; }
};

} // namespace blink

#endif // CSSInterpolationType_h
