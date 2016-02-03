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

    virtual InterpolationValue maybeConvertSingle(const PropertySpecificKeyframe&, const InterpolationEnvironment&, const InterpolationValue& underlying, ConversionCheckers&) const;
    virtual InterpolationValue maybeConvertNeutral(const InterpolationValue& underlying, ConversionCheckers&) const { ASSERT_NOT_REACHED(); return nullptr; }
    virtual InterpolationValue maybeConvertInitial() const { ASSERT_NOT_REACHED(); return nullptr; }
    virtual InterpolationValue maybeConvertInherit(const StyleResolverState&, ConversionCheckers&) const { ASSERT_NOT_REACHED(); return nullptr; }
    virtual InterpolationValue maybeConvertValue(const CSSValue& value, const StyleResolverState&, ConversionCheckers&) const { ASSERT_NOT_REACHED(); return nullptr; }
};

} // namespace blink

#endif // CSSInterpolationType_h
