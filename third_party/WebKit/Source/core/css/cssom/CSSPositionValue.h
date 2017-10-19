// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPositionValue_h
#define CSSPositionValue_h

#include "core/CoreExport.h"
#include "core/css/cssom/CSSStyleValue.h"
#include "platform/bindings/ScriptWrappable.h"

namespace blink {

class CSSNumericValue;
class ExceptionState;

class CORE_EXPORT CSSPositionValue final : public CSSStyleValue {
  WTF_MAKE_NONCOPYABLE(CSSPositionValue);
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Constructor defined in the IDL.
  static CSSPositionValue* Create(CSSNumericValue* x,
                                  CSSNumericValue* y,
                                  ExceptionState&);

  // Getters and setters defined in the IDL.
  CSSNumericValue* x() { return x_.Get(); }
  CSSNumericValue* y() { return y_.Get(); }
  void setX(CSSNumericValue* x, ExceptionState&);
  void setY(CSSNumericValue* x, ExceptionState&);

  // Internal methods - from CSSStyleValue.
  StyleValueType GetType() const final { return kPositionType; }

  const CSSValue* ToCSSValue() const final;

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(x_);
    visitor->Trace(y_);
    CSSStyleValue::Trace(visitor);
  }

 protected:
  CSSPositionValue(CSSNumericValue* x, CSSNumericValue* y) : x_(x), y_(y) {}

  Member<CSSNumericValue> x_;
  Member<CSSNumericValue> y_;
};

}  // namespace blink

#endif
