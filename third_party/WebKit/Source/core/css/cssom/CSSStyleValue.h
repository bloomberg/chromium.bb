// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSStyleValue_h
#define CSSStyleValue_h

#include "core/CSSPropertyNames.h"
#include "core/CoreExport.h"
#include "core/css/CSSValue.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class ExceptionState;
class ScriptState;
class ScriptValue;

// The base class for all CSS values returned by the Typed OM.
// See CSSStyleValue.idl for additional documentation about this class.
class CORE_EXPORT CSSStyleValue
    : public GarbageCollectedFinalized<CSSStyleValue>,
      public ScriptWrappable {
  WTF_MAKE_NONCOPYABLE(CSSStyleValue);
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum StyleValueType {
    kUnknownType,
    kAngleType,
    kFlexType,
    kFrequencyType,
    kKeywordType,
    kLengthType,
    kNumberType,
    kPercentType,
    kPositionType,
    kResolutionType,
    kTimeType,
    kTransformType,
    kUnparsedType,
    kURLImageType,
    kInvalidType,
  };

  static ScriptValue parse(ScriptState*,
                           const String& property_name,
                           const String& value,
                           ExceptionState&);

  virtual ~CSSStyleValue() {}

  virtual StyleValueType GetType() const = 0;
  virtual bool ContainsPercent() const { return false; }

  virtual const CSSValue* ToCSSValue() const = 0;
  virtual const CSSValue* ToCSSValueWithProperty(CSSPropertyID) const {
    return ToCSSValue();
  }
  virtual String toString() const {
    const CSSValue* result = ToCSSValue();
    // TODO(meade): Remove this once all the number and length types are
    // rewritten.
    return result ? result->CssText() : "";
  }

  virtual void Trace(blink::Visitor* visitor) {}

 protected:
  static String StyleValueTypeToString(StyleValueType);

  CSSStyleValue() {}
};

typedef HeapVector<Member<CSSStyleValue>> CSSStyleValueVector;

}  // namespace blink

#endif
