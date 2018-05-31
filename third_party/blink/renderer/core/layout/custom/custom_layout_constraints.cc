// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/custom/custom_layout_constraints.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

CustomLayoutConstraints::CustomLayoutConstraints(LayoutUnit fixed_inline_size,
                                                 SerializedScriptValue* data,
                                                 v8::Isolate* isolate)
    : fixed_inline_size_(fixed_inline_size) {
  if (data)
    layout_worklet_world_v8_data_.Set(isolate, data->Deserialize(isolate));
}

CustomLayoutConstraints::~CustomLayoutConstraints() = default;

ScriptValue CustomLayoutConstraints::data(ScriptState* script_state) const {
  // "data" is *only* exposed to the LayoutWorkletGlobalScope, and we are able
  // to return the same deserialized object. We don't need to check which world
  // it is being accessed from.
  DCHECK(ExecutionContext::From(script_state)->IsLayoutWorkletGlobalScope());
  DCHECK(script_state->World().IsWorkerWorld());

  if (layout_worklet_world_v8_data_.IsEmpty())
    return ScriptValue::CreateNull(script_state);

  return ScriptValue(script_state, layout_worklet_world_v8_data_.NewLocal(
                                       script_state->GetIsolate()));
}

void CustomLayoutConstraints::Trace(blink::Visitor* visitor) {
  visitor->Trace(layout_worklet_world_v8_data_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
