// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ScriptModule.h"

#include "bindings/core/v8/V8Binding.h"
#include "core/dom/Modulator.h"
#include "core/dom/ScriptModuleResolver.h"

namespace blink {

ScriptModule::ScriptModule(v8::Isolate* isolate, v8::Local<v8::Module> module)
    : m_module(SharedPersistent<v8::Module>::create(module, isolate)),
      m_identityHash(static_cast<unsigned>(module->GetIdentityHash())) {
  DCHECK(!m_module->isEmpty());
}

ScriptModule::~ScriptModule() {}

ScriptModule ScriptModule::compile(v8::Isolate* isolate,
                                   const String& source,
                                   const String& fileName,
                                   AccessControlStatus accessControlStatus) {
  v8::TryCatch tryCatch(isolate);
  tryCatch.SetVerbose(true);
  v8::Local<v8::Module> module;
  if (!v8Call(V8ScriptRunner::compileModule(isolate, source, fileName,
                                            accessControlStatus),
              module, tryCatch)) {
    // Compilation error is not used in Blink implementaion logic.
    // Note: Error message is delivered to user (e.g. console) by message
    // listeners set on v8::Isolate. See V8Initializer::initalizeMainThread().
    // TODO(nhiroki): Revisit this when supporting modules on worker threads.
    DCHECK(tryCatch.HasCaught());
    return ScriptModule();
  }
  DCHECK(!tryCatch.HasCaught());
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

Vector<String> ScriptModule::moduleRequests(ScriptState* scriptState) {
  if (isNull())
    return Vector<String>();

  v8::Local<v8::Module> module = m_module->newLocal(scriptState->isolate());

  Vector<String> ret;

  int length = module->GetModuleRequestsLength();
  ret.reserveInitialCapacity(length);
  for (int i = 0; i < length; ++i) {
    v8::Local<v8::String> v8Name = module->GetModuleRequest(i);
    ret.push_back(toCoreString(v8Name));
  }
  return ret;
}

v8::MaybeLocal<v8::Module> ScriptModule::resolveModuleCallback(
    v8::Local<v8::Context> context,
    v8::Local<v8::String> specifier,
    v8::Local<v8::Module> referrer) {
  v8::Isolate* isolate = context->GetIsolate();
  Modulator* modulator = Modulator::from(ScriptState::from(context));
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
