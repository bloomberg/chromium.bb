// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/V8HiddenValue.h"

#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/core/v8/V8Binding.h"

namespace blink {

v8::Local<v8::Value> V8HiddenValue::GetHiddenValue(ScriptState* script_state,
                                                   v8::Local<v8::Object> object,
                                                   v8::Local<v8::String> key) {
  v8::Local<v8::Context> context = script_state->GetContext();
  v8::Local<v8::Private> private_key =
      v8::Private::ForApi(script_state->GetIsolate(), key);
  v8::Local<v8::Value> value;
  // Callsites interpret an empty handle has absence of a result.
  if (!V8CallBoolean(object->HasPrivate(context, private_key)))
    return v8::Local<v8::Value>();
  if (object->GetPrivate(context, private_key).ToLocal(&value))
    return value;
  return v8::Local<v8::Value>();
}

bool V8HiddenValue::SetHiddenValue(ScriptState* script_state,
                                   v8::Local<v8::Object> object,
                                   v8::Local<v8::String> key,
                                   v8::Local<v8::Value> value) {
  if (UNLIKELY(value.IsEmpty()))
    return false;
  return V8CallBoolean(object->SetPrivate(
      script_state->GetContext(),
      v8::Private::ForApi(script_state->GetIsolate(), key), value));
}

bool V8HiddenValue::DeleteHiddenValue(ScriptState* script_state,
                                      v8::Local<v8::Object> object,
                                      v8::Local<v8::String> key) {
  // Actually deleting the value would make force the object into dictionary
  // mode which is unnecessarily slow. Instead, we replace the hidden value with
  // "undefined".
  return V8CallBoolean(
      object->SetPrivate(script_state->GetContext(),
                         v8::Private::ForApi(script_state->GetIsolate(), key),
                         v8::Undefined(script_state->GetIsolate())));
}

}  // namespace blink
