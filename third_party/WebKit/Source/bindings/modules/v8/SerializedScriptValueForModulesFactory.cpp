// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/modules/v8/SerializedScriptValueForModulesFactory.h"

#include "bindings/modules/v8/serialization/V8ScriptValueDeserializerForModules.h"
#include "bindings/modules/v8/serialization/V8ScriptValueSerializerForModules.h"
#include "platform/instrumentation/tracing/TraceEvent.h"

namespace blink {

PassRefPtr<SerializedScriptValue>
SerializedScriptValueForModulesFactory::create(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    const SerializedScriptValue::SerializeOptions& options,
    ExceptionState& exceptionState) {
  TRACE_EVENT0("blink", "SerializedScriptValueFactory::create");
  V8ScriptValueSerializerForModules serializer(ScriptState::current(isolate),
                                               options);
  return serializer.serialize(value, exceptionState);
}

v8::Local<v8::Value> SerializedScriptValueForModulesFactory::deserialize(
    SerializedScriptValue* value,
    v8::Isolate* isolate,
    const SerializedScriptValue::DeserializeOptions& options) {
  TRACE_EVENT0("blink", "SerializedScriptValueFactory::deserialize");
  V8ScriptValueDeserializerForModules deserializer(
      ScriptState::current(isolate), value, options);
  return deserializer.deserialize();
}

}  // namespace blink
