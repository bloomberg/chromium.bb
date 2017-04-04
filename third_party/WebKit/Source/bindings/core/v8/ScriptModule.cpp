// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ScriptModule.h"

#include "bindings/core/v8/V8Binding.h"
#include "core/dom/Modulator.h"
#include "core/dom/ScriptModuleResolver.h"

namespace blink {

ScriptModule::ScriptModule(v8::Isolate* isolate, v8::Local<v8::Module> module)
    : m_module(SharedPersistent<v8::Module>::create(module, isolate)) {}

ScriptModule::~ScriptModule() {}

ScriptModule ScriptModule::compile(v8::Isolate* isolate,
                                   const String& source,
                                   const String& fileName) {
  v8::TryCatch tryCatch(isolate);
  tryCatch.SetVerbose(true);
  v8::Local<v8::Module> module;
  if (!v8Call(V8ScriptRunner::compileModule(isolate, source, fileName), module,
              tryCatch)) {
    // TODO(adamk): Signal failure somehow.
    return ScriptModule(isolate, module);
  }
  return ScriptModule(isolate, module);
}

ScriptValue ScriptModule::instantiate(ScriptState* scriptState) {
  v8::Isolate* isolate = scriptState->isolate();
  v8::TryCatch tryCatch(isolate);

  DCHECK(!isNull());
  v8::Local<v8::Context> context = scriptState->context();
  bool success = m_module->newLocal(scriptState->isolate())
                     ->Instantiate(context, &resolveModuleCallback);
  if (!success) {
    DCHECK(tryCatch.HasCaught());
    return ScriptValue(scriptState, tryCatch.Exception());
  }
  DCHECK(!tryCatch.HasCaught());
  return ScriptValue();
}

void ScriptModule::evaluate(ScriptState* scriptState) {
  v8::Isolate* isolate = scriptState->isolate();
  v8::TryCatch tryCatch(isolate);
  tryCatch.SetVerbose(true);
  v8::Local<v8::Value> result;
  if (!v8Call(V8ScriptRunner::evaluateModule(m_module->newLocal(isolate),
                                             scriptState->context(), isolate),
              result, tryCatch)) {
    // TODO(adamk): report error
  }
}

v8::MaybeLocal<v8::Module> ScriptModule::resolveModuleCallback(
    v8::Local<v8::Context> context,
    v8::Local<v8::String> specifier,
    v8::Local<v8::Module> referrer) {
  v8::Isolate* isolate = context->GetIsolate();
  Modulator* modulator = V8PerContextData::from(context)->modulator();
  DCHECK(modulator);

  ScriptModule referrerRecord(isolate, referrer);
  ExceptionState exceptionState(isolate, ExceptionState::ExecutionContext,
                                "ScriptModule", "resolveModuleCallback");
  ScriptModule resolved = modulator->scriptModuleResolver()->resolve(
      toCoreStringWithNullCheck(specifier), referrerRecord, exceptionState);
  if (resolved.isNull()) {
    DCHECK(exceptionState.hadException());
    return v8::MaybeLocal<v8::Module>();
  }

  DCHECK(!exceptionState.hadException());
  return v8::MaybeLocal<v8::Module>(resolved.m_module->newLocal(isolate));
}

}  // namespace blink
