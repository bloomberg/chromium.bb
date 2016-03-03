// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/StyleValueFactory.h"

#include "core/css/CSSValue.h"
#include "core/css/cssom/SimpleLength.h"
#include "core/css/cssom/StyleValue.h"

namespace blink {

StyleValue* StyleValueFactory::create(CSSPropertyID propertyID, const CSSValue& value)
{
    if (value.isPrimitiveValue()) {
        const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(value);
        if (primitiveValue.isLength() && !primitiveValue.isCalculated()) {
            return SimpleLength::create(primitiveValue.getDoubleValue(), primitiveValue.typeWithCalcResolved());
        }
    }
    // TODO(meade): Implement the rest.
    return nullptr;
}

} // namespace blink
