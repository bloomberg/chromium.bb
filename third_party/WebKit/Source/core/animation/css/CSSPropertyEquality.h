// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPropertyEquality_h
#define CSSPropertyEquality_h

#include "core/CSSPropertyNames.h"

namespace blink {

class LayoutStyle;

class CSSPropertyEquality {
public:
    static bool propertiesEqual(CSSPropertyID, const LayoutStyle&, const LayoutStyle&);
};

} // namespace blink

#endif // CSSPropertyEquality_h
