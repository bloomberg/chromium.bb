// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/serialization/SerializedScriptValueFactory.h"

#include "bindings/core/v8/serialization/V8ScriptValueDeserializer.h"
#include "bindings/core/v8/serialization/V8ScriptValueSerializer.h"
#include "platform/instrumentation/tracing/TraceEvent.h"

namespace blink {

SerializedScriptValueFactory* SerializedScriptValueFactory::instance_ = 0;

PassRefPtr<SerializedScriptValue> SerializedScriptValueFactory::Create(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    const SerializedScriptValue::SerializeOptions& options,
    ExceptionState& exception_state) {
  TRACE_EVENT0("blink", "SerializedScriptValueFactory::create");
  V8ScriptValueSerializer serializer(ScriptState::Current(isolate), options);
  return serializer.Serialize(value, exception_state);
}

v8::Local<v8::Value> SerializedScriptValueFactory::Deserialize(
    RefPtr<SerializedScriptValue> value,
    v8::Isolate* isolate,
    const SerializedScriptValue::DeserializeOptions& options) {
  TRACE_EVENT0("blink", "SerializedScriptValueFactory::deserialize");
  V8ScriptValueDeserializer deserializer(ScriptState::Current(isolate),
                                         std::move(value), options);
  return deserializer.Deserialize();
}

v8::Local<v8::Value> SerializedScriptValueFactory::Deserialize(
    UnpackedSerializedScriptValue* value,
    v8::Isolate* isolate,
    const SerializedScriptValue::DeserializeOptions& options) {
  TRACE_EVENT0("blink", "SerializedScriptValueFactory::deserialize");
  V8ScriptValueDeserializer deserializer(ScriptState::Current(isolate), value,
                                         options);
  return deserializer.Deserialize();
}

}  // namespace blink
