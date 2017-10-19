// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSRayValue_h
#define CSSRayValue_h

#include "core/css/CSSValue.h"

namespace blink {

class CSSIdentifierValue;
class CSSPrimitiveValue;

class CSSRayValue : public CSSValue {
 public:
  static CSSRayValue* Create(const CSSPrimitiveValue& angle,
                             const CSSIdentifierValue& size,
                             const CSSIdentifierValue* contain);

  const CSSPrimitiveValue& Angle() const { return *angle_; }
  const CSSIdentifierValue& Size() const { return *size_; }
  const CSSIdentifierValue* Contain() const { return contain_.Get(); }

  String CustomCSSText() const;

  bool Equals(const CSSRayValue&) const;

  void TraceAfterDispatch(blink::Visitor*);

 private:
  CSSRayValue(const CSSPrimitiveValue& angle,
              const CSSIdentifierValue& size,
              const CSSIdentifierValue* contain);

  Member<const CSSPrimitiveValue> angle_;
  Member<const CSSIdentifierValue> size_;
  Member<const CSSIdentifierValue> contain_;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSRayValue, IsRayValue());

}  // namespace blink

#endif  // CSSRayValue_h
