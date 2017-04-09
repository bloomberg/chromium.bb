/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "bindings/core/v8/V8CustomEvent.h"

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/SerializedScriptValue.h"
#include "bindings/core/v8/SerializedScriptValueFactory.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8CustomEventInit.h"
#include "bindings/core/v8/V8DOMWrapper.h"
#include "bindings/core/v8/V8Event.h"
#include "bindings/core/v8/V8PrivateProperty.h"
#include "core/dom/ContextFeatures.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

static void StoreDetail(ScriptState* script_state,
                        CustomEvent* impl,
                        v8::Local<v8::Object> wrapper,
                        v8::Local<v8::Value> detail) {
  auto private_detail =
      V8PrivateProperty::GetCustomEventDetail(script_state->GetIsolate());
  private_detail.Set(wrapper, detail);

  // When a custom event is created in an isolated world, serialize
  // |detail| and store it in |impl| so that we can clone |detail|
  // when the getter of |detail| is called in the main world later.
  if (script_state->World().IsIsolatedWorld())
    impl->SetSerializedDetail(
        SerializedScriptValue::SerializeAndSwallowExceptions(
            script_state->GetIsolate(), detail));
}

void V8CustomEvent::constructorCustom(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exception_state(
      info.GetIsolate(), ExceptionState::kConstructionContext, "CustomEvent");
  if (UNLIKELY(info.Length() < 1)) {
    exception_state.ThrowTypeError(
        ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  V8StringResource<> type = info[0];
  if (!type.Prepare())
    return;

  CustomEventInit event_init_dict;
  if (!IsUndefinedOrNull(info[1])) {
    if (!info[1]->IsObject()) {
      exception_state.ThrowTypeError(
          "parameter 2 ('eventInitDict') is not an object.");
      return;
    }
    V8CustomEventInit::toImpl(info.GetIsolate(), info[1], event_init_dict,
                              exception_state);
    if (exception_state.HadException())
      return;
  }

  CustomEvent* impl = CustomEvent::Create(type, event_init_dict);
  v8::Local<v8::Object> wrapper = info.Holder();
  wrapper = impl->AssociateWithWrapper(
      info.GetIsolate(), &V8CustomEvent::wrapperTypeInfo, wrapper);

  // TODO(bashi): Workaround for http://crbug.com/529941. We need to store
  // |detail| as a private property to avoid cycle references.
  if (event_init_dict.hasDetail()) {
    v8::Local<v8::Value> v8_detail = event_init_dict.detail().V8Value();
    StoreDetail(ScriptState::Current(info.GetIsolate()), impl, wrapper,
                v8_detail);
  }
  V8SetReturnValue(info, wrapper);
}

void V8CustomEvent::initCustomEventMethodEpilogueCustom(
    const v8::FunctionCallbackInfo<v8::Value>& info,
    CustomEvent* impl) {
  if (impl->IsBeingDispatched())
    return;
  if (info.Length() >= 4) {
    v8::Local<v8::Value> detail = info[3];
    if (!detail.IsEmpty()) {
      StoreDetail(ScriptState::Current(info.GetIsolate()), impl, info.Holder(),
                  detail);
    }
  }
}

void V8CustomEvent::detailAttributeGetterCustom(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  CustomEvent* event = V8CustomEvent::toImpl(info.Holder());
  ScriptState* script_state = ScriptState::Current(isolate);

  auto private_detail = V8PrivateProperty::GetCustomEventDetail(isolate);
  v8::Local<v8::Value> detail = private_detail.GetOrEmpty(info.Holder());
  if (!detail.IsEmpty()) {
    V8SetReturnValue(info, detail);
    return;
  }

  // Be careful not to return a V8 value which is created in different world.
  if (SerializedScriptValue* serialized_value = event->SerializedDetail()) {
    detail = serialized_value->Deserialize(isolate);
  } else if (script_state->World().IsIsolatedWorld()) {
    v8::Local<v8::Value> main_world_detail =
        private_detail.GetFromMainWorld(event);
    if (!main_world_detail.IsEmpty()) {
      event->SetSerializedDetail(
          SerializedScriptValue::SerializeAndSwallowExceptions(
              isolate, main_world_detail));
      detail = event->SerializedDetail()->Deserialize(isolate);
    }
  }

  // |detail| should be null when it is an empty handle because its default
  // value is null.
  if (detail.IsEmpty())
    detail = v8::Null(isolate);
  private_detail.Set(info.Holder(), detail);
  V8SetReturnValue(info, detail);
}

}  // namespace blink
