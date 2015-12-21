// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/StyleColor.h"

#include "core/layout/LayoutTheme.h"

namespace blink {

Color StyleColor::colorFromKeyword(CSSValueID keyword)
{
    if (const char* valueName = getValueName(keyword)) {
        if (const NamedColor* namedColor = findColor(valueName, strlen(valueName)))
            return Color(namedColor->ARGBValue);
    }
    return LayoutTheme::theme().systemColor(keyword);
}

} // namespace blink
