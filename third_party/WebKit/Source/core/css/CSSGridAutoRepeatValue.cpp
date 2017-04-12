// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSGridAutoRepeatValue.h"

#include "platform/wtf/text/StringBuilder.h"

namespace blink {

String CSSGridAutoRepeatValue::CustomCSSText() const {
  StringBuilder result;
  result.Append("repeat(");
  result.Append(getValueName(AutoRepeatID()));
  result.Append(", ");
  result.Append(CSSValueList::CustomCSSText());
  result.Append(')');
  return result.ToString();
}

}  // namespace blink
