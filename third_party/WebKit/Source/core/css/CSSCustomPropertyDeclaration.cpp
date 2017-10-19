// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSCustomPropertyDeclaration.h"

#include "core/css/parser/CSSParserTokenRange.h"

namespace blink {

void CSSCustomPropertyDeclaration::TraceAfterDispatch(blink::Visitor* visitor) {
  CSSValue::TraceAfterDispatch(visitor);
}

String CSSCustomPropertyDeclaration::CustomCSSText() const {
  if (value_)
    return value_->TokenRange().Serialize();
  DCHECK(value_id_ != CSSValueInternalVariableValue);
  return getValueName(value_id_);
}

}  // namespace blink
