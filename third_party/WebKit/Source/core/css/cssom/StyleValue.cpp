// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/StyleValue.h"

#include "bindings/core/v8/ScriptValue.h"

namespace blink {

StyleValue* StyleValue::create(const CSSValue& val)
{
    // TODO: implement.
    return nullptr;
}

ScriptValue StyleValue::parse(ScriptState* state, const String& property, const String& cssText)
{
    // TODO: implement.
    return ScriptValue();
}

} // namespace blink
