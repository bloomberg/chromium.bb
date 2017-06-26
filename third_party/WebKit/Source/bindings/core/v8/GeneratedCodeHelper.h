// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is for functions that are used only by generated code.
// CAUTION:
// All functions defined in this file should be used by generated code only.
// If you want to use them from hand-written code, please find appropriate
// location and move them to that location.

#ifndef GeneratedCodeHelper_h
#define GeneratedCodeHelper_h

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/CoreExport.h"
#include "platform/wtf/PassRefPtr.h"
#include "v8/include/v8.h"

namespace blink {

class ScriptState;
class SerializedScriptValue;

CORE_EXPORT void V8ConstructorAttributeGetter(
    v8::Local<v8::Name> property_name,
    const v8::PropertyCallbackInfo<v8::Value>&);

CORE_EXPORT v8::Local<v8::Value> V8Deserialize(
    v8::Isolate*,
    PassRefPtr<SerializedScriptValue>);

// ExceptionToRejectPromiseScope converts a possible exception to a reject
// promise and returns the promise instead of throwing the exception.
//
// Promise-returning DOM operations are required to always return a promise
// and to never throw an exception.
// See also http://heycam.github.io/webidl/#es-operations
class CORE_EXPORT ExceptionToRejectPromiseScope {
  STACK_ALLOCATED();

 public:
  ExceptionToRejectPromiseScope(const v8::FunctionCallbackInfo<v8::Value>& info,
                                ExceptionState& exception_state)
      : info_(info), exception_state_(exception_state) {}
  ~ExceptionToRejectPromiseScope() {
    if (!exception_state_.HadException())
      return;

    // As exceptions must always be created in the current realm, reject
    // promises must also be created in the current realm while regular promises
    // are created in the relevant realm of the context object.
    ScriptState* script_state = ScriptState::ForFunctionObject(info_);
    V8SetReturnValue(info_, exception_state_.Reject(script_state).V8Value());
  }

 private:
  const v8::FunctionCallbackInfo<v8::Value>& info_;
  ExceptionState& exception_state_;
};

using InstallTemplateFunction =
    void (*)(v8::Isolate* isolate,
             const DOMWrapperWorld& world,
             v8::Local<v8::FunctionTemplate> interface_template);

using InstallRuntimeEnabledFeaturesFunction =
    void (*)(v8::Isolate*,
             const DOMWrapperWorld&,
             v8::Local<v8::Object> instance,
             v8::Local<v8::Object> prototype,
             v8::Local<v8::Function> interface);

using InstallRuntimeEnabledFeaturesOnTemplateFunction = InstallTemplateFunction;

}  // namespace blink

#endif  // GeneratedCodeHelper_h
