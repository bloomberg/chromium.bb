// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSUnsetValue_h
#define CSSUnsetValue_h

#include "core/css/CSSValue.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

class CSSUnsetValue : public CSSValue {
 public:
  static CSSUnsetValue* Create();

  String CustomCSSText() const;

  bool Equals(const CSSUnsetValue&) const { return true; }

  DEFINE_INLINE_TRACE_AFTER_DISPATCH() {
    CSSValue::TraceAfterDispatch(visitor);
  }

 private:
  friend class CSSValuePool;

  CSSUnsetValue() : CSSValue(kUnsetClass) {}
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSUnsetValue, IsUnsetValue());

}  // namespace blink

#endif  // CSSUnsetValue_h
