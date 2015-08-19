// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LengthPropertyFunctions_h
#define LengthPropertyFunctions_h

#include "core/CSSPropertyNames.h"
#include "core/CSSValueKeywords.h"
#include "platform/Length.h"
#include "wtf/Allocator.h"

namespace blink {

class ComputedStyle;

class LengthPropertyFunctions {
    STATIC_ONLY(LengthPropertyFunctions);
public:
    typedef void (ComputedStyle::*LengthSetter)(const Length&);

    static ValueRange valueRange(CSSPropertyID);
    static bool getPixelsForKeyword(CSSPropertyID, CSSValueID, double& resultPixels);
    static bool getLength(CSSPropertyID, const ComputedStyle&, Length& result);
    static bool getInitialLength(CSSPropertyID, Length& result);
};

} // namespace blink

#endif // LengthPropertyFunctions_h
