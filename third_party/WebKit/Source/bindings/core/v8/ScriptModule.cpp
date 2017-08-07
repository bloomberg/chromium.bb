// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ScriptModule.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/Modulator.h"
#include "core/dom/ScriptModuleResolver.h"
#include "core/probe/CoreProbes.h"

namespace blink {

const char* ScriptModuleStateToString(ScriptModuleState state) {
  switch (state) {
    case ScriptModuleState::kUninstantiated:
      return "uninstantiated";
    case ScriptModuleState::kInstantiating:
      return "instantinating";
    case ScriptModuleState::kInstantiated:
      return "instantiated";
    case ScriptModuleState::kEvaluating:
      return "evaluating";
    case ScriptModuleState::kEvaluated:
      return "evaluated";
    case ScriptModuleState::kErrored:
      return "errored";
  }
  NOTREACHED();
  return "";
}

ScriptModule::ScriptModule() {
  // We ensure module-related code is not executed without the flag.
  // https://crbug.com/715376
  CHECK(RuntimeEnabledFeatures::ModuleScriptsEnabled());
}

ScriptModule::ScriptModule(WTF::HashTableDeletedValueType)
    : module_(WTF::kHashTableDeletedValue) {
  // We ensure module-related code is not executed without the flag.
  // https://crbug.com/715376
  CHECK(RuntimeEnabledFeatures::ModuleScriptsEnabled());
}

ScriptModule::ScriptModule(v8::Isolate* isolate, v8::Local<v8::Module> module)
    : module_(SharedPersistent<v8::Module>::Create(module, isolate)),
      identity_hash_(static_cast<unsigned>(module->GetIdentityHash())) {
  // We ensure module-related code is not executed without the flag.
  // https://crbug.com/715376
  CHECK(RuntimeEnabledFeatures::ModuleScriptsEnabled());

  DCHECK(!module_->IsEmpty());
}

ScriptModule::~ScriptModule() {}

ScriptModule ScriptModule::Compile(v8::Isolate* isolate,
                                   const String& source,
                                   const String& file_name,
                                   AccessControlStatus access_control_status,
                                   const TextPosition& start_position,
                                   ExceptionState& exception_state) {
  // We ensure module-related code is not executed without the flag.
  // https://crbug.com/715376
  CHECK(RuntimeEnabledFeatures::ModuleScriptsEnabled());

  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Module> module;
  if (!V8ScriptRunner::CompileModule(isolate, source, file_name,
                                     access_control_status, start_position)
           .ToLocal(&module)) {
    DCHECK(try_catch.HasCaught());
    exception_state.RethrowV8Exception(try_catch.Exception());
    return ScriptModule();
  }
  DCHECK(!try_catch.HasCaught());
  return ScriptModule(isolate, module);
}

ScriptValue ScriptModule::Instantiate(ScriptState* script_state) {
  v8::Isolate* isolate = script_state->GetIsolate();
  v8::TryCatch try_catch(isolate);
  try_catch.SetVerbose(true);

  DCHECK(!IsNull());
  v8::Local<v8::Context> context = script_state->GetContext();
  bool success;
  if (!NewLocal(script_state->GetIsolate())
           ->InstantiateModule(context, &ResolveModuleCallback)
           .To(&success) ||
      !success) {
    DCHECK(try_catch.HasCaught());
    return ScriptValue(script_state, try_catch.Exception());
  }
  DCHECK(!try_catch.HasCaught());
  return ScriptValue();
}

void ScriptModule::Evaluate(ScriptState* script_state) const {
  v8::Isolate* isolate = script_state->GetIsolate();

  // Isolate exceptions that occur when executing the code. These exceptions
  // should not interfere with javascript code we might evaluate from C++ when
  // returning from here.
  v8::TryCatch try_catch(isolate);
  try_catch.SetVerbose(true);

  probe::ExecuteScript probe(ExecutionContext::From(script_state));
  // TODO(kouhei): We currently don't have a code-path which use return value of
  // EvaluateModule. Stop ignoring result once we have such path.
  v8::Local<v8::Value> result;
  if (!V8ScriptRunner::EvaluateModule(module_->NewLocal(isolate),
                                      script_state->GetContext(), isolate)
           .ToLocal(&result)) {
    return;
  }
}

void ScriptModule::ReportException(ScriptState* script_state,
                                   v8::Local<v8::Value> exception,
                                   const String& file_name,
                                   const TextPosition& start_position) {
  // We ensure module-related code is not executed without the flag.
  // https://crbug.com/715376
  CHECK(RuntimeEnabledFeatures::ModuleScriptsEnabled());

  v8::Isolate* isolate = script_state->GetIsolate();

  v8::TryCatch try_catch(isolate);
  try_catch.SetVerbose(true);

  V8ScriptRunner::ReportExceptionForModule(isolate, exception, file_name,
                                           start_position);
}

Vector<String> ScriptModule::ModuleRequests(ScriptState* script_state) {
  if (IsNull())
    return Vector<String>();

  v8::Local<v8::Module> module = module_->NewLocal(script_state->GetIsolate());

  Vector<String> ret;

  int length = module->GetModuleRequestsLength();
  ret.ReserveInitialCapacity(length);
  for (int i = 0; i < length; ++i) {
    v8::Local<v8::String> v8_name = module->GetModuleRequest(i);
    ret.push_back(ToCoreString(v8_name));
  }
  return ret;
}

Vector<TextPosition> ScriptModule::ModuleRequestPositions(
    ScriptState* script_state) {
  if (IsNull())
    return Vector<TextPosition>();
  v8::Local<v8::Module> module = module_->NewLocal(script_state->GetIsolate());

  Vector<TextPosition> ret;

  int length = module->GetModuleRequestsLength();
  ret.ReserveInitialCapacity(length);
  for (int i = 0; i < length; ++i) {
    v8::Location v8_loc = module->GetModuleRequestLocation(i);
    ret.emplace_back(OrdinalNumber::FromZeroBasedInt(v8_loc.GetLineNumber()),
                     OrdinalNumber::FromZeroBasedInt(v8_loc.GetColumnNumber()));
  }
  return ret;
}

ScriptModuleState ScriptModule::Status(ScriptState* script_state) {
  DCHECK(!IsNull());

  v8::Local<v8::Module> module = module_->NewLocal(script_state->GetIsolate());
  return module->GetStatus();
}

v8::Local<v8::Value> ScriptModule::ErrorCompletion(ScriptState* script_state) {
  DCHECK(!IsNull());
  DCHECK_EQ(ScriptModuleState::kErrored, Status(script_state));

  v8::Local<v8::Module> module = module_->NewLocal(script_state->GetIsolate());
  return module->GetException();
}

v8::Local<v8::Value> ScriptModule::V8Namespace(v8::Isolate* isolate) {
  DCHECK(!IsNull());
  v8::Local<v8::Module> module = module_->NewLocal(isolate);
  DCHECK_EQ(ScriptModuleState::kEvaluated, module->GetStatus());
  return module->GetModuleNamespace();
}

v8::MaybeLocal<v8::Module> ScriptModule::ResolveModuleCallback(
    v8::Local<v8::Context> context,
    v8::Local<v8::String> specifier,
    v8::Local<v8::Module> referrer) {
  // We ensure module-related code is not executed without the flag.
  // https://crbug.com/715376
  CHECK(RuntimeEnabledFeatures::ModuleScriptsEnabled());

  v8::Isolate* isolate = context->GetIsolate();
  Modulator* modulator = Modulator::From(ScriptState::From(context));
  DCHECK(modulator);

  ScriptModule referrer_record(isolate, referrer);
  ExceptionState exception_state(isolate, ExceptionState::kExecutionContext,
                                 "ScriptModule", "resolveModuleCallback");
  ScriptModule resolved = modulator->GetScriptModuleResolver()->Resolve(
      ToCoreStringWithNullCheck(specifier), referrer_record, exception_state);
  DCHECK(!resolved.IsNull());
  DCHECK(!exception_state.HadException());
  return resolved.module_->NewLocal(isolate);
}

}  // namespace blink
