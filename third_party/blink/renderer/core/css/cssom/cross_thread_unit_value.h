// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_CROSS_THREAD_UNIT_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_CROSS_THREAD_UNIT_VALUE_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/cssom/cross_thread_style_value.h"

namespace blink {

class CSSStyleValue;

class CORE_EXPORT CrossThreadUnitValue final : public CrossThreadStyleValue {
 public:
  explicit CrossThreadUnitValue(double value, CSSPrimitiveValue::UnitType unit)
      : value_(value), unit_(unit) {}
  ~CrossThreadUnitValue() override = default;

  CSSStyleValue* ToCSSStyleValue() override;

 private:
  friend class CrossThreadStyleValueTest;

  double value_;
  CSSPrimitiveValue::UnitType unit_;
  DISALLOW_COPY_AND_ASSIGN(CrossThreadUnitValue);
};

}  // namespace blink

#endif
