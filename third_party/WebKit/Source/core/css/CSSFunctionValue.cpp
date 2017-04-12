// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSFunctionValue.h"

#include "platform/wtf/text/StringBuilder.h"

namespace blink {

String CSSFunctionValue::CustomCSSText() const {
  StringBuilder result;
  result.Append(getValueName(value_id_));
  result.Append('(');
  result.Append(CSSValueList::CustomCSSText());
  result.Append(')');
  return result.ToString();
}

}  // namespace blink
