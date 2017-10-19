// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSCounterValue.h"

#include "core/css/CSSMarkup.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

namespace cssvalue {

String CSSCounterValue::CustomCSSText() const {
  StringBuilder result;
  if (Separator().IsEmpty())
    result.Append("counter(");
  else
    result.Append("counters(");

  result.Append(Identifier());
  if (!Separator().IsEmpty()) {
    result.Append(", ");
    result.Append(separator_->CssText());
  }
  bool is_default_list_style = ListStyle() == CSSValueDecimal;
  if (!is_default_list_style) {
    result.Append(", ");
    result.Append(list_style_->CssText());
  }
  result.Append(')');

  return result.ToString();
}

void CSSCounterValue::TraceAfterDispatch(blink::Visitor* visitor) {
  visitor->Trace(identifier_);
  visitor->Trace(list_style_);
  visitor->Trace(separator_);
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace cssvalue

}  // namespace blink
