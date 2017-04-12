// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSNumberValue_h
#define CSSNumberValue_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/cssom/CSSStyleValue.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class CORE_EXPORT CSSNumberValue final : public CSSStyleValue {
  WTF_MAKE_NONCOPYABLE(CSSNumberValue);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CSSNumberValue* Create(double value) {
    return new CSSNumberValue(value);
  }

  double value() const { return value_; }

  CSSValue* ToCSSValue() const override {
    return CSSPrimitiveValue::Create(value_,
                                     CSSPrimitiveValue::UnitType::kNumber);
  }

  StyleValueType GetType() const override {
    return StyleValueType::kNumberType;
  }

 private:
  CSSNumberValue(double value) : value_(value) {}

  double value_;
};

}  // namespace blink

#endif
