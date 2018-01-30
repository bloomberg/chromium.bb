// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSLayoutFunctionValue_h
#define CSSLayoutFunctionValue_h

#include "base/macros.h"
#include "core/css/CSSValue.h"
#include "platform/heap/Handle.h"

namespace blink {

class CSSCustomIdentValue;

namespace cssvalue {

class CSSLayoutFunctionValue : public CSSValue {
 public:
  static CSSLayoutFunctionValue* Create(CSSCustomIdentValue* name,
                                        bool is_inline) {
    return new CSSLayoutFunctionValue(name, is_inline);
  }
  ~CSSLayoutFunctionValue();

  String CustomCSSText() const;
  AtomicString GetName() const;
  bool IsInline() const { return is_inline_; }

  bool Equals(const CSSLayoutFunctionValue&) const;
  void TraceAfterDispatch(blink::Visitor*);

 private:
  CSSLayoutFunctionValue(CSSCustomIdentValue* name, bool is_inline);

  Member<CSSCustomIdentValue> name_;
  bool is_inline_;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSLayoutFunctionValue, IsLayoutFunctionValue());

}  // namespace cssvalue
}  // namespace blink

#endif  // CSSLayoutFunctionValue_h
