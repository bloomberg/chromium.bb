// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSInterpolationType_h
#define CSSInterpolationType_h

#include "core/animation/InterpolationEnvironment.h"
#include "core/animation/InterpolationType.h"
#include "core/css/CSSPropertyMetadata.h"

namespace blink {

class CSSInterpolationType : public InterpolationType {
protected:
    CSSInterpolationType(CSSPropertyID property)
        : InterpolationType(PropertyHandle(property))
    { }

    CSSPropertyID cssProperty() const { return property().cssProperty(); }

    virtual PassOwnPtr<InterpolationValue> maybeConvertSingle(const PropertySpecificKeyframe&, const InterpolationEnvironment&, const UnderlyingValue&, ConversionCheckers&) const;
    virtual PassOwnPtr<InterpolationValue> maybeConvertNeutral(const UnderlyingValue&, ConversionCheckers&) const { ASSERT_NOT_REACHED(); return nullptr; }
    virtual PassOwnPtr<InterpolationValue> maybeConvertInitial() const { ASSERT_NOT_REACHED(); return nullptr; }
    virtual PassOwnPtr<InterpolationValue> maybeConvertInherit(const StyleResolverState&, ConversionCheckers&) const { ASSERT_NOT_REACHED(); return nullptr; }
    virtual PassOwnPtr<InterpolationValue> maybeConvertValue(const CSSValue& value, const StyleResolverState&, ConversionCheckers&) const { ASSERT_NOT_REACHED(); return nullptr; }
};

} // namespace blink

#endif // CSSInterpolationType_h
