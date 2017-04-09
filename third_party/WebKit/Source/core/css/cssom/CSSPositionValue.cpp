// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSPositionValue.h"

#include "core/css/CSSValuePair.h"
#include "core/css/cssom/CSSLengthValue.h"

namespace blink {

CSSValue* CSSPositionValue::ToCSSValue() const {
  return CSSValuePair::Create(x_->ToCSSValue(), y_->ToCSSValue(),
                              CSSValuePair::kKeepIdenticalValues);
}

}  // namespace blink
