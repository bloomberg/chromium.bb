// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_AXIS_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_AXIS_VALUE_H_

#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"

namespace blink {

class CSSAxisValue : public CSSValueList {
 public:
  static CSSAxisValue* Create(CSSValueID axis_name) {
    return MakeGarbageCollected<CSSAxisValue>(axis_name);
  }
  static CSSAxisValue* Create(double x, double y, double z) {
    return MakeGarbageCollected<CSSAxisValue>(x, y, z);
  }

  explicit CSSAxisValue(CSSValueID axis_name);
  CSSAxisValue(double x, double y, double z);

  String CustomCSSText() const;

  double X() const;

  double Y() const;

  double Z() const;

  void TraceAfterDispatch(blink::Visitor* visitor) {
    CSSValueList::TraceAfterDispatch(visitor);
  }

 private:
  CSSValueID axis_name_;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSAxisValue, IsAxisValue());

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_AXIS_VALUE_H_
