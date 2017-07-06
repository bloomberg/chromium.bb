// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/modules/v8/serialization/SerializedScriptValueForModulesFactory.h"

#include "bindings/modules/v8/serialization/V8ScriptValueDeserializerForModules.h"
#include "bindings/modules/v8/serialization/V8ScriptValueSerializerForModules.h"
#include "platform/instrumentation/tracing/TraceEvent.h"

namespace blink {

PassRefPtr<SerializedScriptValue>
SerializedScriptValueForModulesFactory::Create(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    const SerializedScriptValue::SerializeOptions& options,
    ExceptionState& exception_state) {
  TRACE_EVENT0("blink", "SerializedScriptValueFactory::create");
  V8ScriptValueSerializerForModules serializer(ScriptState::Current(isolate),
                                               options);
  return serializer.Serialize(value, exception_state);
}

v8::Local<v8::Value> SerializedScriptValueForModulesFactory::Deserialize(
    RefPtr<SerializedScriptValue> value,
    v8::Isolate* isolate,
    const SerializedScriptValue::DeserializeOptions& options) {
  TRACE_EVENT0("blink", "SerializedScriptValueFactory::deserialize");
  V8ScriptValueDeserializerForModules deserializer(
      ScriptState::Current(isolate), std::move(value), options);
  return deserializer.Deserialize();
}

v8::Local<v8::Value> SerializedScriptValueForModulesFactory::Deserialize(
    UnpackedSerializedScriptValue* value,
    v8::Isolate* isolate,
    const SerializedScriptValue::DeserializeOptions& options) {
  TRACE_EVENT0("blink", "SerializedScriptValueFactory::deserialize");
  V8ScriptValueDeserializerForModules deserializer(
      ScriptState::Current(isolate), value, options);
  return deserializer.Deserialize();
}

}  // namespace blink
