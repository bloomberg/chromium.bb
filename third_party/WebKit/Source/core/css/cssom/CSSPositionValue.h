// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPositionValue_h
#define CSSPositionValue_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/css/cssom/CSSStyleValue.h"

namespace blink {

class CSSLengthValue;

class CORE_EXPORT CSSPositionValue final : public CSSStyleValue {
  WTF_MAKE_NONCOPYABLE(CSSPositionValue);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CSSPositionValue* Create(const CSSLengthValue* x,
                                  const CSSLengthValue* y) {
    return new CSSPositionValue(x, y);
  }

  // Bindings require a non const return value.
  CSSLengthValue* x() const { return const_cast<CSSLengthValue*>(x_.Get()); }
  CSSLengthValue* y() const { return const_cast<CSSLengthValue*>(y_.Get()); }

  StyleValueType GetType() const override { return kPositionType; }

  CSSValue* ToCSSValue() const override;

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(x_);
    visitor->Trace(y_);
    CSSStyleValue::Trace(visitor);
  }

 protected:
  CSSPositionValue(const CSSLengthValue* x, const CSSLengthValue* y)
      : x_(x), y_(y) {}

  Member<const CSSLengthValue> x_;
  Member<const CSSLengthValue> y_;
};

}  // namespace blink

#endif
