// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSStringValue.h"

#include "core/css/CSSMarkup.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

CSSStringValue::CSSStringValue(const String& str)
    : CSSValue(kStringClass), string_(str) {}

String CSSStringValue::CustomCSSText() const {
  return SerializeString(string_);
}

DEFINE_TRACE_AFTER_DISPATCH(CSSStringValue) {
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace blink
