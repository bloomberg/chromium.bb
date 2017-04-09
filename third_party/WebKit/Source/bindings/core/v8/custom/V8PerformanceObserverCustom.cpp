// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/V8PerformanceObserver.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/PerformanceObserverCallback.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8DOMWrapper.h"
#include "bindings/core/v8/V8GCController.h"
#include "bindings/core/v8/V8Performance.h"
#include "bindings/core/v8/V8PrivateProperty.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/timing/DOMWindowPerformance.h"
#include "core/timing/PerformanceObserver.h"

namespace blink {

void V8PerformanceObserver::constructorCustom(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(
        isolate, ExceptionMessages::FailedToConstruct(
                     "PerformanceObserver",
                     ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  v8::Local<v8::Object> wrapper = info.Holder();

  Performance* performance = nullptr;
  LocalDOMWindow* window = ToLocalDOMWindow(wrapper->CreationContext());
  if (!window) {
    V8ThrowException::ThrowTypeError(
        isolate, ExceptionMessages::FailedToConstruct(
                     "PerformanceObserver", "No 'window' in current context."));
    return;
  }
  performance = DOMWindowPerformance::performance(*window);
  ASSERT(performance);

  if (info.Length() <= 0 || !info[0]->IsFunction()) {
    V8ThrowException::ThrowTypeError(
        isolate,
        ExceptionMessages::FailedToConstruct(
            "PerformanceObserver",
            "The callback provided as parameter 1 is not a function."));
    return;
  }
  ScriptState* script_state = ScriptState::ForReceiverObject(info);
  v8::Local<v8::Function> v8_callback = v8::Local<v8::Function>::Cast(info[0]);
  PerformanceObserverCallback* callback =
      PerformanceObserverCallback::Create(script_state, v8_callback);

  PerformanceObserver* observer = PerformanceObserver::Create(
      CurrentExecutionContext(isolate), performance, callback);

  // TODO(bashi): Don't set private property (and remove this custom
  // constructor) when we can trace correctly. See crbug.com/468240.
  V8PrivateProperty::GetPerformanceObserverCallback(isolate).Set(wrapper,
                                                                 v8_callback);
  V8SetReturnValue(info, V8DOMWrapper::AssociateObjectWithWrapper(
                             isolate, observer, &wrapperTypeInfo, wrapper));
}

}  // namespace blink
