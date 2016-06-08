// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/StyleValueFactory.h"

#include "core/css/CSSValue.h"
#include "core/css/cssom/CSSSimpleLength.h"
#include "core/css/cssom/CSSStyleValue.h"

namespace blink {

CSSStyleValueVector StyleValueFactory::cssValueToStyleValueVector(CSSPropertyID propertyID, const CSSValue& value)
{
    CSSStyleValueVector styleValueVector;

    if (value.isPrimitiveValue()) {
        const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(value);
        if (primitiveValue.isLength() && !primitiveValue.isCalculated()) {
            styleValueVector.append(CSSSimpleLength::create(primitiveValue.getDoubleValue(), primitiveValue.typeWithCalcResolved()));
            return styleValueVector;
        }
    }
    // TODO(meade): Implement the rest.
    return styleValueVector;
}

} // namespace blink
