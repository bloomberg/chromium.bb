// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "core/timing/PerformanceMark.h"

#include "bindings/core/v8/serialization/SerializedScriptValue.h"
#include "bindings/core/v8/serialization/SerializedScriptValueFactory.h"

namespace blink {

PerformanceMark::PerformanceMark(ScriptState* script_state,
                                 const String& name,
                                 double start_time,
                                 const ScriptValue& detail)
    : PerformanceEntry(name, "mark", start_time, start_time) {
  world_ = WrapRefCounted(&script_state->World());
  if (detail.IsEmpty() || detail.IsNull() || detail.IsUndefined()) {
    return;
  }
  detail_.Set(detail.GetIsolate(), detail.V8Value());
}

ScriptValue PerformanceMark::detail(ScriptState* script_state) const {
  v8::Isolate* isolate = script_state->GetIsolate();
  if (detail_.IsEmpty()) {
    return ScriptValue(script_state, v8::Null(isolate));
  }
  // Return a serialized clone when the world is different.
  if (!world_ || world_->GetWorldId() != script_state->World().GetWorldId()) {
    v8::Local<v8::Value> value = detail_.NewLocal(isolate);
    scoped_refptr<SerializedScriptValue> serialized =
        SerializedScriptValue::SerializeAndSwallowExceptions(isolate, value);
    return ScriptValue(script_state, serialized->Deserialize(isolate));
  }
  return ScriptValue(script_state, detail_.NewLocal(isolate));
}

void PerformanceMark::Trace(blink::Visitor* visitor) {
  PerformanceEntry::Trace(visitor);
}

void PerformanceMark::TraceWrappers(
    const ScriptWrappableVisitor* visitor) const {
  visitor->TraceWrappers(detail_);
  PerformanceEntry::TraceWrappers(visitor);
}

}  // namespace blink
