// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/modules/v8/webgl_any.h"

#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

ScriptValue WebGLAny(ScriptState* script_state, bool value) {
  return ScriptValue(script_state,
                     v8::Boolean::New(script_state->GetIsolate(), value));
}

ScriptValue WebGLAny(ScriptState* script_state,
                     const bool* value,
                     uint32_t size) {
  v8::Local<v8::Array> array = v8::Array::New(script_state->GetIsolate(), size);
  for (uint32_t i = 0; i < size; ++i) {
    if (!V8CallBoolean(array->CreateDataProperty(
            script_state->GetContext(), i,
            v8::Boolean::New(script_state->GetIsolate(), value[i]))))
      return ScriptValue();
  }
  return ScriptValue(script_state, array);
}

ScriptValue WebGLAny(ScriptState* script_state, const Vector<bool>& value) {
  wtf_size_t size = value.size();
  v8::Local<v8::Array> array = v8::Array::New(script_state->GetIsolate(), size);
  for (wtf_size_t i = 0; i < size; ++i) {
    if (!V8CallBoolean(array->CreateDataProperty(
            script_state->GetContext(), i,
            v8::Boolean::New(script_state->GetIsolate(), value[i]))))
      return ScriptValue();
  }
  return ScriptValue(script_state, array);
}

ScriptValue WebGLAny(ScriptState* script_state, const Vector<unsigned>& value) {
  wtf_size_t size = value.size();
  v8::Local<v8::Array> array = v8::Array::New(script_state->GetIsolate(), size);
  for (wtf_size_t i = 0; i < size; ++i) {
    if (!V8CallBoolean(array->CreateDataProperty(
            script_state->GetContext(), i,
            v8::Integer::NewFromUnsigned(script_state->GetIsolate(),
                                         value[i]))))
      return ScriptValue();
  }
  return ScriptValue(script_state, array);
}

ScriptValue WebGLAny(ScriptState* script_state, const Vector<int>& value) {
  wtf_size_t size = value.size();
  v8::Local<v8::Array> array = v8::Array::New(script_state->GetIsolate(), size);
  for (wtf_size_t i = 0; i < size; ++i) {
    if (!V8CallBoolean(array->CreateDataProperty(
            script_state->GetContext(), i,
            v8::Integer::New(script_state->GetIsolate(), value[i]))))
      return ScriptValue();
  }
  return ScriptValue(script_state, array);
}

ScriptValue WebGLAny(ScriptState* script_state, int value) {
  return ScriptValue(script_state,
                     v8::Integer::New(script_state->GetIsolate(), value));
}

ScriptValue WebGLAny(ScriptState* script_state, unsigned value) {
  return ScriptValue(
      script_state, v8::Integer::NewFromUnsigned(script_state->GetIsolate(),
                                                 static_cast<unsigned>(value)));
}

ScriptValue WebGLAny(ScriptState* script_state, int64_t value) {
  return ScriptValue(script_state, v8::Number::New(script_state->GetIsolate(),
                                                   static_cast<double>(value)));
}

ScriptValue WebGLAny(ScriptState* script_state, uint64_t value) {
  return ScriptValue(script_state, v8::Number::New(script_state->GetIsolate(),
                                                   static_cast<double>(value)));
}

ScriptValue WebGLAny(ScriptState* script_state, float value) {
  return ScriptValue(script_state,
                     v8::Number::New(script_state->GetIsolate(), value));
}

ScriptValue WebGLAny(ScriptState* script_state, String value) {
  return ScriptValue(script_state, V8String(script_state->GetIsolate(), value));
}

ScriptValue WebGLAny(ScriptState* script_state, WebGLObject* value) {
  return ScriptValue(script_state,
                     ToV8(value, script_state->GetContext()->Global(),
                          script_state->GetIsolate()));
}

ScriptValue WebGLAny(ScriptState* script_state, DOMFloat32Array* value) {
  return ScriptValue(script_state,
                     ToV8(value, script_state->GetContext()->Global(),
                          script_state->GetIsolate()));
}

ScriptValue WebGLAny(ScriptState* script_state, DOMInt32Array* value) {
  return ScriptValue(script_state,
                     ToV8(value, script_state->GetContext()->Global(),
                          script_state->GetIsolate()));
}

ScriptValue WebGLAny(ScriptState* script_state, DOMUint8Array* value) {
  return ScriptValue(script_state,
                     ToV8(value, script_state->GetContext()->Global(),
                          script_state->GetIsolate()));
}

ScriptValue WebGLAny(ScriptState* script_state, DOMUint32Array* value) {
  return ScriptValue(script_state,
                     ToV8(value, script_state->GetContext()->Global(),
                          script_state->GetIsolate()));
}

}  // namespace blink
