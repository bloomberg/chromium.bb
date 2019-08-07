// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/core/timing/performance_mark.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value_factory.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/core/timing/performance_mark_options.h"
#include "third_party/blink/renderer/core/timing/performance_user_timing.h"

namespace blink {

PerformanceMark::PerformanceMark(ScriptState* script_state,
                                 const AtomicString& name,
                                 double start_time,
                                 const ScriptValue& detail)
    : PerformanceEntry(name, start_time, start_time) {
  world_ = WrapRefCounted(&script_state->World());
  if (detail.IsEmpty() || detail.IsNull() || detail.IsUndefined()) {
    return;
  }
  detail_.Set(detail.GetIsolate(), detail.V8Value());
}

// static
PerformanceMark* PerformanceMark::Create(ScriptState* script_state,
                                         const AtomicString& mark_name,
                                         PerformanceMarkOptions* mark_options,
                                         ExceptionState& exception_state) {
  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  if (!window)
    return nullptr;
  Performance* performance = DOMWindowPerformance::performance(*window);
  if (!performance)
    return nullptr;

  return performance->GetUserTiming().CreatePerformanceMark(
      script_state, mark_name, mark_options, exception_state);
}

AtomicString PerformanceMark::entryType() const {
  return performance_entry_names::kMark;
}

PerformanceEntryType PerformanceMark::EntryTypeEnum() const {
  return PerformanceEntry::EntryType::kMark;
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
  visitor->Trace(detail_);
  PerformanceEntry::Trace(visitor);
}

}  // namespace blink
