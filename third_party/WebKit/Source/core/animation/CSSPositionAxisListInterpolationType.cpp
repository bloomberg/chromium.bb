// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSPositionAxisListInterpolationType.h"

#include "core/animation/CSSLengthInterpolationType.h"
#include "core/animation/ListInterpolationFunctions.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/CSSValuePair.h"

namespace blink {

static InterpolationValue convertPositionAxisCSSValue(const CSSValue& value)
{
    if (!value.isValuePair())
        return CSSLengthInterpolationType::maybeConvertCSSValue(value);

    const CSSValuePair& pair = toCSSValuePair(value);
    InterpolationValue result = CSSLengthInterpolationType::maybeConvertCSSValue(pair.second());
    CSSValueID side = toCSSPrimitiveValue(pair.first()).getValueID();
    if (side == CSSValueRight || side == CSSValueBottom)
        CSSLengthInterpolationType::subtractFromOneHundredPercent(result);
    return result;
}

InterpolationValue CSSPositionAxisListInterpolationType::maybeConvertValue(const CSSValue& value, const StyleResolverState&, ConversionCheckers&) const
{
    if (!value.isBaseValueList()) {
        return ListInterpolationFunctions::createList(1, [&value](size_t) {
            return convertPositionAxisCSSValue(value);
        });
    }

    const CSSValueList& list = toCSSValueList(value);
    return ListInterpolationFunctions::createList(list.length(), [&list](size_t index) {
        return convertPositionAxisCSSValue(*list.item(index));
    });
}

} // namespace blink
