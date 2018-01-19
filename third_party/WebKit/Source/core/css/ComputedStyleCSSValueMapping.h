// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ComputedStyleCSSValueMapping_h
#define ComputedStyleCSSValueMapping_h

#include "core/css/CSSValue.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

class CSSVariableData;
class ComputedStyle;
class PropertyRegistry;

class ComputedStyleCSSValueMapping {
  STATIC_ONLY(ComputedStyleCSSValueMapping);

 public:
  static const CSSValue* Get(const AtomicString custom_property_name,
                             const ComputedStyle&,
                             const PropertyRegistry*);
  static std::unique_ptr<HashMap<AtomicString, scoped_refptr<CSSVariableData>>>
  GetVariables(const ComputedStyle&);
};

}  // namespace blink

#endif  // ComputedStyleCSSValueMapping_h
