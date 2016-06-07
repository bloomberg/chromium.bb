// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StyleValueFactory_h
#define StyleValueFactory_h

#include "core/CSSPropertyNames.h"
#include "wtf/Allocator.h"

namespace blink {

class CSSValue;
class CSSStyleValue;

class StyleValueFactory {
    STATIC_ONLY(StyleValueFactory);

public:
    static CSSStyleValue* create(CSSPropertyID, const CSSValue&);
};

} // namespace blink

#endif
