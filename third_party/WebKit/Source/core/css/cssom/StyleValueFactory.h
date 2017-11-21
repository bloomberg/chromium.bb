// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StyleValueFactory_h
#define StyleValueFactory_h

#include "core/CSSPropertyNames.h"
#include "core/css/cssom/CSSStyleValue.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class CSSValue;

class CORE_EXPORT StyleValueFactory {
  STATIC_ONLY(StyleValueFactory);

 public:
  static CSSStyleValueVector FromString(CSSPropertyID,
                                        const String&,
                                        SecureContextMode);
  static CSSStyleValueVector CssValueToStyleValueVector(CSSPropertyID,
                                                        const CSSValue&);
  // If you don't have complex CSS properties, use this one.
  static CSSStyleValueVector CssValueToStyleValueVector(const CSSValue&);
};

}  // namespace blink

#endif
