// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSNumericValue.h"

#include "core/css/CSSPrimitiveValue.h"
#include "core/css/cssom/CSSUnitValue.h"

namespace blink {

CSSNumericValue* CSSNumericValue::parse(const String& css_text,
                                        ExceptionState&) {
  // TODO(meade): Implement
  return nullptr;
}

CSSNumericValue* CSSNumericValue::FromCSSValue(const CSSPrimitiveValue& value) {
  if (value.IsCalculated()) {
    // TODO(meade): Implement this case.
    return nullptr;
  }
  return CSSUnitValue::FromCSSValue(value);
}

}  // namespace blink
