// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSIdentifierValue_h
#define CSSIdentifierValue_h

#include "core/CSSValueKeywords.h"
#include "core/css/CSSValue.h"
#include "core/css/CSSValueIDMappings.h"
#include "platform/wtf/TypeTraits.h"

namespace blink {

// CSSIdentifierValue stores CSS value keywords, e.g. 'none', 'auto',
// 'lower-roman'.
// TODO(sashab): Rename this class to CSSKeywordValue once it no longer
// conflicts with CSSOM's CSSKeywordValue class.
class CORE_EXPORT CSSIdentifierValue : public CSSValue {
 public:
  static CSSIdentifierValue* Create(CSSValueID);

  // TODO(sashab): Rename this to createFromPlatformValue().
  template <typename T>
  static CSSIdentifierValue* Create(T value) {
    static_assert(!std::is_same<T, CSSValueID>::value,
                  "Do not call create() with a CSSValueID; call "
                  "createIdentifier() instead");
    return new CSSIdentifierValue(value);
  }

  static CSSIdentifierValue* Create(const Length& value) {
    return new CSSIdentifierValue(value);
  }

  CSSValueID GetValueID() const { return value_id_; }

  String CustomCSSText() const;

  bool Equals(const CSSIdentifierValue& other) const {
    return value_id_ == other.value_id_;
  }

  template <typename T>
  inline T ConvertTo()
      const {  // Overridden for special cases in CSSPrimitiveValueMappings.h
    return CssValueIDToPlatformEnum<T>(value_id_);
  }

  void TraceAfterDispatch(blink::Visitor*);

 private:
  explicit CSSIdentifierValue(CSSValueID);

  // TODO(sashab): Remove this function, and update mapping methods to
  // specialize the create() method instead.
  template <typename T>
  CSSIdentifierValue(
      T t)  // Overriden for special cases in CSSPrimitiveValueMappings.h
      : CSSValue(kIdentifierClass), value_id_(PlatformEnumToCSSValueID(t)) {}

  CSSIdentifierValue(const Length&);

  CSSValueID value_id_;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSIdentifierValue, IsIdentifierValue());

}  // namespace blink

#endif  // CSSIdentifierValue_h
