// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/V8IntersectionObserver.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/IntersectionObserverCallback.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8GCController.h"
#include "bindings/core/v8/V8IntersectionObserverDelegate.h"
#include "bindings/core/v8/V8IntersectionObserverInit.h"
#include "core/dom/Element.h"
#include "platform/bindings/V8DOMWrapper.h"

namespace blink {

void V8IntersectionObserver::constructorCustom(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exception_state(info.GetIsolate(),
                                 ExceptionState::kConstructionContext,
                                 "IntersectionObserver");

  if (UNLIKELY(info.Length() < 1)) {
    exception_state.ThrowTypeError(
        ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  v8::Local<v8::Object> wrapper = info.Holder();

  if (!info[0]->IsFunction()) {
    exception_state.ThrowTypeError(
        "The callback provided as parameter 1 is not a function.");
    return;
  }

  if (info.Length() > 1 && !IsUndefinedOrNull(info[1]) &&
      !info[1]->IsObject()) {
    exception_state.ThrowTypeError("parameter 2 ('options') is not an object.");
    return;
  }

  IntersectionObserverInit intersection_observer_init;
  V8IntersectionObserverInit::toImpl(
      info.GetIsolate(), info[1], intersection_observer_init, exception_state);
  if (exception_state.HadException())
    return;

  IntersectionObserverCallback* callback = IntersectionObserverCallback::Create(
      ScriptState::Current(info.GetIsolate()), info[0]);
  V8IntersectionObserverDelegate* delegate = new V8IntersectionObserverDelegate(
      callback, ScriptState::Current(info.GetIsolate()));
  IntersectionObserver* observer = IntersectionObserver::Create(
      intersection_observer_init, *delegate, exception_state);
  if (exception_state.HadException())
    return;
  DCHECK(observer);
  V8SetReturnValue(info,
                   V8DOMWrapper::AssociateObjectWithWrapper(
                       info.GetIsolate(), observer, &wrapperTypeInfo, wrapper));
}

}  // namespace blink
