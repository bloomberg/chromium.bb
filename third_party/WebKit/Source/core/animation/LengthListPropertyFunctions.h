// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LengthListPropertyFunctions_h
#define LengthListPropertyFunctions_h

#include "core/CSSPropertyNames.h"
#include "core/style/ComputedStyle.h"
#include "platform/Length.h"
#include "wtf/Allocator.h"
#include "wtf/RefVector.h"

namespace blink {

class LengthListPropertyFunctions {
    STATIC_ONLY(LengthListPropertyFunctions);
public:
    static ValueRange valueRange(CSSPropertyID property)
    {
        ASSERT(property == CSSPropertyStrokeDasharray);
        return ValueRangeNonNegative;
    }

    static const RefVector<Length>* getInitialLengthList(CSSPropertyID property)
    {
        ASSERT(property == CSSPropertyStrokeDasharray);
        return nullptr;
    }

    static const RefVector<Length>* getLengthList(CSSPropertyID property, const ComputedStyle& style)
    {
        ASSERT(property == CSSPropertyStrokeDasharray);
        return style.strokeDashArray();
    }

    static void setLengthList(CSSPropertyID property, ComputedStyle& style, PassRefPtr<RefVector<Length>> lengthList)
    {
        ASSERT(property == CSSPropertyStrokeDasharray);
        style.setStrokeDashArray(lengthList);
    }

};

} // namespace blink

#endif // LengthListPropertyFunctions_h
