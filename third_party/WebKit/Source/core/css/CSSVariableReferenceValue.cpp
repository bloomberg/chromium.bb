// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSVariableReferenceValue.h"

namespace blink {

DEFINE_TRACE_AFTER_DISPATCH(CSSVariableReferenceValue) {
  CSSValue::TraceAfterDispatch(visitor);
  visitor->Trace(parser_context_);
}

String CSSVariableReferenceValue::CustomCSSText() const {
  // We may want to consider caching this value.
  return data_->TokenRange().Serialize();
}

}  // namespace blink
