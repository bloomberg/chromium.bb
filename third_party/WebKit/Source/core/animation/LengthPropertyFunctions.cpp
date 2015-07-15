// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/LengthPropertyFunctions.h"

#include "core/style/ComputedStyle.h"

namespace blink {

// TODO(alancutter): Generate these functions.

ValueRange LengthPropertyFunctions::valueRange(CSSPropertyID property)
{
    ASSERT(property == CSSPropertyLeft);
    return ValueRangeAll;
}

bool LengthPropertyFunctions::getPixelsForKeyword(CSSPropertyID property, CSSValueID valueID, double& result)
{
    ASSERT(property == CSSPropertyLeft);
    return false;
}

bool LengthPropertyFunctions::getLength(CSSPropertyID property, const ComputedStyle& style, Length& result)
{
    ASSERT(property == CSSPropertyLeft);
    result = style.left();
    return true;
}

bool LengthPropertyFunctions::getInitialLength(CSSPropertyID property, Length& result)
{
    return getLength(property, *ComputedStyle::initialStyle(), result);
}

} // namespace blink
