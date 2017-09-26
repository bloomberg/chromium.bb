// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8ScriptValueSerializerTest_h
#define V8ScriptValueSerializerTest_h

#include "bindings/core/v8/ExceptionState.h"
#include "platform/bindings/ScriptState.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Vector.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

RefPtr<SerializedScriptValue> SerializedValue(const Vector<uint8_t>& bytes);

// Checks for a DOM exception, including a rethrown one.
::testing::AssertionResult HadDOMException(const StringView& name,
                                           ScriptState* script_state,
                                           ExceptionState& exception_state);

}  // namespace blink

#endif  // V8ScriptValueSerializerTest_h
