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

class CORE_EXPORT CSSPositionValue final : public CSSStyleValue {
  WTF_MAKE_NONCOPYABLE(CSSPositionValue);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CSSPositionValue* Create(const CSSNumericValue* x,
                                  const CSSNumericValue* y) {
    return new CSSPositionValue(x, y);
  }

  // Bindings require a non const return value.
  CSSNumericValue* x() const { return const_cast<CSSNumericValue*>(x_.Get()); }
  CSSNumericValue* y() const { return const_cast<CSSNumericValue*>(y_.Get()); }

  StyleValueType GetType() const override { return kPositionType; }

  CSSValue* ToCSSValue() const override;

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(x_);
    visitor->Trace(y_);
    CSSStyleValue::Trace(visitor);
  }

 protected:
  CSSPositionValue(const CSSNumericValue* x, const CSSNumericValue* y)
      : x_(x), y_(y) {}

  Member<const CSSNumericValue> x_;
  Member<const CSSNumericValue> y_;
};

}  // namespace blink

#endif
