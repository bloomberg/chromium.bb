// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/JSONValuesForV8.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8Binding.h"

namespace blink {

v8::Local<v8::Value> fromJSONString(ScriptState* scriptState,
                                    const String& stringifiedJSON,
                                    ExceptionState& exceptionState) {
  v8::Isolate* isolate = scriptState->isolate();
  v8::Local<v8::Value> parsed;
  v8::TryCatch tryCatch(isolate);
  if (!v8Call(v8::JSON::Parse(isolate, v8String(isolate, stringifiedJSON)),
              parsed, tryCatch)) {
    if (tryCatch.HasCaught())
      exceptionState.rethrowV8Exception(tryCatch.Exception());
  }

  return parsed;
}

}  // namespace blink
