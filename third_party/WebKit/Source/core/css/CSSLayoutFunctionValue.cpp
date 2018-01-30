// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSLayoutFunctionValue.h"

#include "core/css/CSSCustomIdentValue.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {
namespace cssvalue {

CSSLayoutFunctionValue::CSSLayoutFunctionValue(CSSCustomIdentValue* name,
                                               bool is_inline)
    : CSSValue(kLayoutFunctionClass), name_(name), is_inline_(is_inline) {}

CSSLayoutFunctionValue::~CSSLayoutFunctionValue() = default;

String CSSLayoutFunctionValue::CustomCSSText() const {
  StringBuilder result;
  if (is_inline_)
    result.Append("inline-");
  result.Append("layout(");
  result.Append(name_->CustomCSSText());
  result.Append(')');
  return result.ToString();
}

AtomicString CSSLayoutFunctionValue::GetName() const {
  return name_->Value();
}

bool CSSLayoutFunctionValue::Equals(const CSSLayoutFunctionValue& other) const {
  return GetName() == other.GetName() && IsInline() == other.IsInline();
}

void CSSLayoutFunctionValue::TraceAfterDispatch(blink::Visitor* visitor) {
  visitor->Trace(name_);
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace cssvalue
}  // namespace blink
