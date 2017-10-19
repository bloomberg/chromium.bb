// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSColorValue_h
#define CSSColorValue_h

#include "core/css/CSSValue.h"
#include "platform/graphics/Color.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class CSSValuePool;

namespace cssvalue {

// Represents the non-keyword subset of <color>.
class CSSColorValue : public CSSValue {
 public:
  // TODO(sashab): Make this create() method take a Color instead.
  static CSSColorValue* Create(RGBA32 color);

  String CustomCSSText() const {
    return color_.SerializedAsCSSComponentValue();
  }

  Color Value() const { return color_; }

  bool Equals(const CSSColorValue& other) const {
    return color_ == other.color_;
  }

  void TraceAfterDispatch(blink::Visitor* visitor) {
    CSSValue::TraceAfterDispatch(visitor);
  }

 private:
  friend class ::blink::CSSValuePool;

  CSSColorValue(Color color) : CSSValue(kColorClass), color_(color) {}

  Color color_;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSColorValue, IsColorValue());

}  // namespace cssvalue
}  // namespace blink

#endif  // CSSColorValue_h
