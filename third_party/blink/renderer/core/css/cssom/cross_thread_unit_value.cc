// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/cross_thread_unit_value.h"

#include "third_party/blink/renderer/core/css/cssom/css_unit_value.h"

namespace blink {

CSSStyleValue* CrossThreadUnitValue::ToCSSStyleValue() {
  return CSSUnitValue::Create(value_, unit_);
}

}  // namespace blink
