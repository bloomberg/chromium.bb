/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "bindings/core/v8/V8DOMWrapper.h"

#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8Location.h"
#include "bindings/core/v8/V8ObjectConstructor.h"
#include "bindings/core/v8/V8PerContextData.h"
#include "bindings/core/v8/V8PerIsolateData.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "bindings/core/v8/V8Window.h"
#include "core/dom/Document.h"
#include "core/frame/LocalDOMWindow.h"

namespace blink {

v8::Local<v8::Object> V8DOMWrapper::CreateWrapper(
    v8::Isolate* isolate,
    v8::Local<v8::Object> creation_context,
    const WrapperTypeInfo* type) {
  ASSERT(!type->Equals(&V8Window::wrapperTypeInfo));
  // According to
  // https://html.spec.whatwg.org/multipage/browsers.html#security-location,
  // cross-origin script access to a few properties of Location is allowed.
  // Location already implements the necessary security checks.
  bool with_security_check = !type->Equals(&V8Location::wrapperTypeInfo);
  V8WrapperInstantiationScope scope(creation_context, isolate,
                                    with_security_check);

  V8PerContextData* per_context_data =
      V8PerContextData::From(scope.GetContext());
  v8::Local<v8::Object> wrapper;
  if (per_context_data) {
    wrapper = per_context_data->CreateWrapperFromCache(type);
  } else {
    // The context is detached, but still accessible.
    // TODO(yukishiino): This code does not create a wrapper with
    // the correct settings.  Should follow the same way as
    // V8PerContextData::createWrapperFromCache, though there is no need to
    // cache resulting objects or their constructors.
    const DOMWrapperWorld& world = DOMWrapperWorld::World(scope.GetContext());
    wrapper = type->domTemplate(isolate, world)
                  ->InstanceTemplate()
                  ->NewInstance(scope.GetContext())
                  .ToLocalChecked();
  }
  return wrapper;
}

bool V8DOMWrapper::IsWrapper(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  if (value.IsEmpty() || !value->IsObject())
    return false;
  v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(value);

  if (object->InternalFieldCount() < kV8DefaultWrapperInternalFieldCount)
    return false;

  const WrapperTypeInfo* untrusted_wrapper_type_info =
      ToWrapperTypeInfo(object);
  V8PerIsolateData* per_isolate_data = V8PerIsolateData::From(isolate);
  if (!(untrusted_wrapper_type_info && per_isolate_data))
    return false;
  return per_isolate_data->HasInstance(untrusted_wrapper_type_info, object);
}

bool V8DOMWrapper::HasInternalFieldsSet(v8::Local<v8::Value> value) {
  if (value.IsEmpty() || !value->IsObject())
    return false;
  v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(value);

  if (object->InternalFieldCount() < kV8DefaultWrapperInternalFieldCount)
    return false;

  const ScriptWrappable* untrusted_script_wrappable = ToScriptWrappable(object);
  const WrapperTypeInfo* untrusted_wrapper_type_info =
      ToWrapperTypeInfo(object);
  return untrusted_script_wrappable && untrusted_wrapper_type_info &&
         untrusted_wrapper_type_info->gin_embedder == gin::kEmbedderBlink;
}

void V8WrapperInstantiationScope::SecurityCheck(
    v8::Isolate* isolate,
    v8::Local<v8::Context> context_for_wrapper) {
  if (context_.IsEmpty())
    return;
  // If the context is different, we need to make sure that the current
  // context has access to the creation context.
  LocalFrame* frame = ToLocalFrameIfNotDetached(context_for_wrapper);
  if (!frame) {
    // Sandbox detached frames - they can't create cross origin objects.
    LocalDOMWindow* calling_window = CurrentDOMWindow(isolate);
    LocalDOMWindow* target_window = ToLocalDOMWindow(context_for_wrapper);
    // TODO(jochen): Currently, Location is the only object for which we can
    // reach this code path. Should be generalized.
    ExceptionState exception_state(
        isolate, ExceptionState::kConstructionContext, "Location");
    if (BindingSecurity::ShouldAllowAccessToDetachedWindow(
            calling_window, target_window, exception_state))
      return;

    CHECK_EQ(kSecurityError, exception_state.Code());
    return;
  }
  const DOMWrapperWorld& current_world = DOMWrapperWorld::World(context_);
  RELEASE_ASSERT(current_world.GetWorldId() ==
                 DOMWrapperWorld::World(context_for_wrapper).GetWorldId());
  // TODO(jochen): Add the interface name here once this is generalized.
  ExceptionState exception_state(isolate, ExceptionState::kConstructionContext,
                                 nullptr);
  if (current_world.IsMainWorld() &&
      !BindingSecurity::ShouldAllowAccessToFrame(CurrentDOMWindow(isolate),
                                                 frame, exception_state)) {
    CHECK_EQ(kSecurityError, exception_state.Code());
    return;
  }
}

void V8WrapperInstantiationScope::ConvertException() {
  v8::Isolate* isolate = context_->GetIsolate();
  // TODO(jochen): Currently, Location is the only object for which we can reach
  // this code path. Should be generalized.
  ExceptionState exception_state(isolate, ExceptionState::kConstructionContext,
                                 "Location");
  LocalDOMWindow* calling_window = CurrentDOMWindow(isolate);
  LocalDOMWindow* target_window = ToLocalDOMWindow(context_);
  exception_state.ThrowSecurityError(
      target_window->SanitizedCrossDomainAccessErrorMessage(calling_window),
      target_window->CrossDomainAccessErrorMessage(calling_window));
}

}  // namespace blink
