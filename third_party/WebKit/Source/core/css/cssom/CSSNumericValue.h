// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSNumericValue_h
#define CSSNumericValue_h

#include "bindings/core/v8/double_or_css_numeric_value.h"
#include "core/CoreExport.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/cssom/CSSNumericValueType.h"
#include "core/css/cssom/CSSStyleValue.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class CSSUnitValue;
class ExceptionState;

using CSSNumberish = DoubleOrCSSNumericValue;

class CORE_EXPORT CSSNumericValue : public CSSStyleValue {
  WTF_MAKE_NONCOPYABLE(CSSNumericValue);
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Blink-internal ways of creating CSSNumericValues.
  static CSSNumericValue* parse(const String& css_text, ExceptionState&);
  static CSSNumericValue* FromCSSValue(const CSSPrimitiveValue&);
  // https://drafts.css-houdini.org/css-typed-om-1/#rectify-a-numberish-value
  static CSSNumericValue* FromNumberish(const CSSNumberish& value);

  // Methods defined in the IDL.
  virtual CSSNumericValue* add(const CSSNumericValue*, ExceptionState&) {
    // TODO(meade): Implement.
    return nullptr;
  }
  virtual CSSNumericValue* sub(const CSSNumericValue*, ExceptionState&) {
    // TODO(meade): Implement.
    return nullptr;
  }
  virtual CSSNumericValue* mul(double, ExceptionState&) {
    // TODO(meade): Implement.
    return nullptr;
  }
  virtual CSSNumericValue* div(double, ExceptionState&) {
    // TODO(meade): Implement.
    return nullptr;
  }
  // Converts between compatible types, as defined in the IDL.
  CSSNumericValue* to(const String&, ExceptionState&);

  // Internal methods.
  // Converts between compatible types.
  virtual CSSUnitValue* to(CSSPrimitiveValue::UnitType) const = 0;
  virtual bool IsCalculated() const = 0;

  const CSSNumericValueType& Type() const { return type_; }

 protected:
  static bool IsValidUnit(CSSPrimitiveValue::UnitType);
  static CSSPrimitiveValue::UnitType UnitFromName(const String& name);

  CSSNumericValue(const CSSNumericValueType& type) : type_(type) {}

  CSSNumericValueType type_;
};

using CSSNumericValueVector = HeapVector<Member<CSSNumericValue>>;

}  // namespace blink

#endif  // CSSNumericValue_h
