// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/GeneratedCodeHelper.h"

#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/serialization/SerializedScriptValue.h"
#include "core/dom/ExecutionContext.h"

namespace blink {

void V8ConstructorAttributeGetter(
    v8::Local<v8::Name> property_name,
    const v8::PropertyCallbackInfo<v8::Value>& info,
    const WrapperTypeInfo* wrapper_type_info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(
      info.GetIsolate(), "Blink_V8ConstructorAttributeGetter");
  V8PerContextData* per_context_data =
      V8PerContextData::From(info.Holder()->CreationContext());
  if (!per_context_data)
    return;
  V8SetReturnValue(info,
                   per_context_data->ConstructorForType(wrapper_type_info));
}

v8::Local<v8::Value> V8Deserialize(v8::Isolate* isolate,
                                   scoped_refptr<SerializedScriptValue> value) {
  if (value)
    return value->Deserialize(isolate);
  return v8::Null(isolate);
}

bool IsCallbackFunctionRunnable(
    const ScriptState* callback_relevant_script_state) {
  if (!callback_relevant_script_state->ContextIsValid())
    return false;
  const ExecutionContext* execution_context =
      ExecutionContext::From(callback_relevant_script_state);
  return execution_context && !execution_context->IsContextPaused() &&
         !execution_context->IsContextDestroyed();
}

}  // namespace blink
