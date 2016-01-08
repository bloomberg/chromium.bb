// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/TransformValue.h"

namespace blink {

bool TransformValue::is2D() const
{
    // TODO: implement;
    return true;
}

String TransformValue::cssString() const
{
    // TODO: implement
    return "cssString";
}

PassRefPtrWillBeRawPtr<CSSValue> TransformValue::toCSSValue() const
{
    // TODO: implement
    return nullptr;
}

} // namespace blink
