// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8IteratorResultValue_h
#define V8IteratorResultValue_h

#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/ToV8ForCore.h"
#include "core/CoreExport.h"
#include "v8/include/v8.h"

namespace blink {

// "Iterator result" in this file is an object returned from iterator.next()
// having two members "done" and "value".

CORE_EXPORT v8::Local<v8::Object> V8IteratorResultValue(v8::Isolate*,
                                                        bool done,
                                                        v8::Local<v8::Value>);

// Unpacks |result|, stores the value of "done" member to |done| and returns
// the value of "value" member. Returns an empty handle when errored.
CORE_EXPORT v8::MaybeLocal<v8::Value>
V8UnpackIteratorResult(ScriptState*, v8::Local<v8::Object> result, bool* done);

template <typename T>
inline ScriptValue V8IteratorResult(ScriptState* script_state, const T& value) {
  return ScriptValue(
      script_state,
      V8IteratorResultValue(script_state->GetIsolate(), false,
                            ToV8(value, script_state->GetContext()->Global(),
                                 script_state->GetIsolate())));
}

inline ScriptValue V8IteratorResultDone(ScriptState* script_state) {
  return ScriptValue(
      script_state,
      V8IteratorResultValue(script_state->GetIsolate(), true,
                            v8::Undefined(script_state->GetIsolate())));
}

}  // namespace blink

#endif  // V8IteratorResultValue_h
