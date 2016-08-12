// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSStyleImageValue.h"

namespace blink {

double CSSStyleImageValue::intrinsicWidth(bool& isNull)
{
    isNull = isCachePending();
    if (isNull)
        return 0;
    return imageLayoutSize().width().toDouble();
}

double CSSStyleImageValue::intrinsicHeight(bool& isNull)
{
    isNull = isCachePending();
    if (isNull)
        return 0;
    return imageLayoutSize().height().toDouble();
}

double CSSStyleImageValue::intrinsicRatio(bool& isNull)
{
    isNull = isCachePending();
    if (isNull) {
        return 0;
    }
    if (intrinsicHeight(isNull) == 0) {
        isNull = true;
        return 0;
    }
    return intrinsicWidth(isNull) / intrinsicHeight(isNull);
}

} // namespace blink
