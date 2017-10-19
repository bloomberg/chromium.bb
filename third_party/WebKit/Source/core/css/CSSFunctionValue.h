// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSFunctionValue_h
#define CSSFunctionValue_h

#include "core/CSSValueKeywords.h"
#include "core/css/CSSValueList.h"

namespace blink {

class CSSFunctionValue : public CSSValueList {
 public:
  static CSSFunctionValue* Create(CSSValueID id) {
    return new CSSFunctionValue(id);
  }

  String CustomCSSText() const;

  bool Equals(const CSSFunctionValue& other) const {
    return value_id_ == other.value_id_ && CSSValueList::Equals(other);
  }
  CSSValueID FunctionType() const { return value_id_; }

  void TraceAfterDispatch(blink::Visitor* visitor) {
    CSSValueList::TraceAfterDispatch(visitor);
  }

 private:
  CSSFunctionValue(CSSValueID id)
      : CSSValueList(kFunctionClass, kCommaSeparator), value_id_(id) {}

  const CSSValueID value_id_;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSFunctionValue, IsFunctionValue());

}  // namespace blink

#endif
