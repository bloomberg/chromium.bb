// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSStyleValue_h
#define CSSStyleValue_h

#include "base/macros.h"
#include "bindings/core/v8/Nullable.h"
#include "core/CSSPropertyNames.h"
#include "core/CoreExport.h"
#include "core/css/CSSValue.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class ExceptionState;
class ExecutionContext;
enum class SecureContextMode;

class CSSStyleValue;
using CSSStyleValueVector = HeapVector<Member<CSSStyleValue>>;

// The base class for all CSS values returned by the Typed OM.
// See CSSStyleValue.idl for additional documentation about this class.
class CORE_EXPORT CSSStyleValue : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum StyleValueType {
    kUnknownType,
    kAngleType,
    kFlexType,
    kFrequencyType,
    kInvertType,
    kKeywordType,
    kLengthType,
    kMaxType,
    kMinType,
    kNegateType,
    kNumberType,
    kPercentType,
    kPositionType,
    kProductType,
    kResolutionType,
    kSumType,
    kTimeType,
    kTransformType,
    kUnparsedType,
    kURLImageType,
    kInvalidType,
  };

  static CSSStyleValue* parse(const ExecutionContext*,
                              const String& property_name,
                              const String& value,
                              ExceptionState&);
  static Nullable<CSSStyleValueVector> parseAll(const ExecutionContext*,
                                                const String& property_name,
                                                const String& value,
                                                ExceptionState&);

  virtual ~CSSStyleValue() = default;

  virtual StyleValueType GetType() const = 0;
  virtual bool ContainsPercent() const { return false; }

  virtual const CSSValue* ToCSSValue() const = 0;
  virtual const CSSValue* ToCSSValueWithProperty(CSSPropertyID) const {
    return ToCSSValue();
  }
  virtual String toString() const;

 protected:
  static String StyleValueTypeToString(StyleValueType);

  CSSStyleValue() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(CSSStyleValue);
};

}  // namespace blink

#endif
