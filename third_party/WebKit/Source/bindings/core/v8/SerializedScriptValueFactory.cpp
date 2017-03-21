// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/SerializedScriptValueFactory.h"

#include "bindings/core/v8/serialization/V8ScriptValueDeserializer.h"
#include "bindings/core/v8/serialization/V8ScriptValueSerializer.h"
#include "platform/instrumentation/tracing/TraceEvent.h"

namespace blink {

SerializedScriptValueFactory* SerializedScriptValueFactory::m_instance = 0;

PassRefPtr<SerializedScriptValue> SerializedScriptValueFactory::create(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    const SerializedScriptValue::SerializeOptions& options,
    ExceptionState& exceptionState) {
  TRACE_EVENT0("blink", "SerializedScriptValueFactory::create");
  V8ScriptValueSerializer serializer(ScriptState::current(isolate), options);
  return serializer.serialize(value, exceptionState);
}

v8::Local<v8::Value> SerializedScriptValueFactory::deserialize(
    SerializedScriptValue* value,
    v8::Isolate* isolate,
    const SerializedScriptValue::DeserializeOptions& options) {
  TRACE_EVENT0("blink", "SerializedScriptValueFactory::deserialize");
  V8ScriptValueDeserializer deserializer(ScriptState::current(isolate), value,
                                         options);
  return deserializer.deserialize();
}

}  // namespace blink
