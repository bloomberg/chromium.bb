// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSUnsetValue.h"

#include "core/css/CSSValuePool.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {
namespace cssvalue {

CSSUnsetValue* CSSUnsetValue::Create() {
  return CssValuePool().UnsetValue();
}

String CSSUnsetValue::CustomCSSText() const {
  return "unset";
}

}  // namespace cssvalue
}  // namespace blink
