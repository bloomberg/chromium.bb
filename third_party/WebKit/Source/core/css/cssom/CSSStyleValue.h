// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSStyleValue_h
#define CSSStyleValue_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CSSPropertyNames.h"
#include "core/CoreExport.h"
#include "core/css/CSSValue.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExceptionState;
class ScriptState;
class ScriptValue;

class CORE_EXPORT CSSStyleValue
    : public GarbageCollectedFinalized<CSSStyleValue>,
      public ScriptWrappable {
  WTF_MAKE_NONCOPYABLE(CSSStyleValue);
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum StyleValueType {
    // This list corresponds to each non-abstract subclass.
    kUnknown,
    kAngleType,
    kCalcLengthType,
    kKeywordType,
    kNumberType,
    kPositionType,
    kSimpleLengthType,
    kTransformType,
    kUnitType,
    kUnparsedType,
    kURLImageType,
  };

  virtual ~CSSStyleValue() {}

  virtual StyleValueType GetType() const = 0;

  static ScriptValue parse(ScriptState*,
                           const String& property_name,
                           const String& value,
                           ExceptionState&);

  virtual const CSSValue* ToCSSValue() const = 0;
  virtual const CSSValue* ToCSSValueWithProperty(CSSPropertyID) const {
    return ToCSSValue();
  }
  virtual String cssText() const { return ToCSSValue()->CssText(); }

  DEFINE_INLINE_VIRTUAL_TRACE() {}

 protected:
  CSSStyleValue() {}
};

typedef HeapVector<Member<CSSStyleValue>> CSSStyleValueVector;

}  // namespace blink

#endif
