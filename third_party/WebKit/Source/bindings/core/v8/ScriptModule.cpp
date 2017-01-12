// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ScriptModule.h"

#include "bindings/core/v8/V8Binding.h"

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

v8::MaybeLocal<v8::Module> dummyCallback(v8::Local<v8::Context> context,
                                         v8::Local<v8::String> specifier,
                                         v8::Local<v8::Module> referrer) {
  return v8::MaybeLocal<v8::Module>();
}

bool ScriptModule::instantiate(ScriptState* scriptState) {
  DCHECK(!isNull());
  v8::Local<v8::Context> context = scriptState->context();
  // TODO(adamk): pass in a real callback.
  return m_module->newLocal(scriptState->isolate())
      ->Instantiate(context, &dummyCallback);
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

}  // namespace blink
