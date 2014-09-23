// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/AnimationInputHelpers.h"

#include "core/css/parser/CSSParser.h"
#include "core/css/resolver/CSSToStyleMap.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

CSSPropertyID AnimationInputHelpers::camelCaseCSSPropertyNameToID(const String& propertyName)
{
    if (propertyName.find('-') != kNotFound)
        return CSSPropertyInvalid;

    StringBuilder builder;
    size_t position = 0;
    size_t end;
    while ((end = propertyName.find(isASCIIUpper, position)) != kNotFound) {
        builder.append(propertyName.substring(position, end - position) + "-" + toASCIILower((propertyName)[end]));
        position = end + 1;
    }
    builder.append(propertyName.substring(position));
    // Doesn't handle prefixed properties.
    CSSPropertyID id = cssPropertyID(builder.toString());
    return id;
}

PassRefPtr<TimingFunction> AnimationInputHelpers::parseTimingFunction(const String& string)
{
    if (string.isEmpty())
        return nullptr;

    RefPtrWillBeRawPtr<CSSValue> value = CSSParser::parseSingleValue(CSSPropertyTransitionTimingFunction, string);
    if (!value || value->isInitialValue() || value->isInheritedValue())
        return nullptr;
    CSSValueList* valueList = toCSSValueList(value.get());
    if (valueList->length() > 1)
        return nullptr;
    return CSSToStyleMap::mapAnimationTimingFunction(valueList->item(0), true);
}

} // namespace blink
