// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ModuleScript.h"

#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "v8/include/v8.h"

namespace blink {

void ModuleScript::SetInstantiationErrorAndClearRecord(ScriptValue error) {
  // Implements Step 7.1 of:
  // https://html.spec.whatwg.org/multipage/webappapis.html#internal-module-script-graph-fetching-procedure

  // "set script's instantiation state to "errored", ..."
  DCHECK_EQ(instantiation_state_, ModuleInstantiationState::kUninstantiated);
  instantiation_state_ = ModuleInstantiationState::kErrored;

  // "its instantiation error to instantiationStatus.[[Value]], and ..."
  DCHECK(!error.IsEmpty());
  {
    ScriptState::Scope scope(error.GetScriptState());
    instantiation_error_.Set(error.GetIsolate(), error.V8Value());
  }

  // "its module record to null."
  record_ = ScriptModule();
}

void ModuleScript::SetInstantiationSuccess() {
  // Implements Step 7.2 of:
  // https://html.spec.whatwg.org/multipage/webappapis.html#internal-module-script-graph-fetching-procedure

  // "set script's instantiation state to "instantiated"."
  DCHECK_EQ(instantiation_state_, ModuleInstantiationState::kUninstantiated);
  instantiation_state_ = ModuleInstantiationState::kInstantiated;
}

DEFINE_TRACE(ModuleScript) {
  visitor->Trace(settings_object_);
  Script::Trace(visitor);
}
DEFINE_TRACE_WRAPPERS(ModuleScript) {
  visitor->TraceWrappers(instantiation_error_);
}

bool ModuleScript::IsEmpty() const {
  return false;
}

bool ModuleScript::CheckMIMETypeBeforeRunScript(Document* context_document,
                                                const SecurityOrigin*) const {
  // We don't check MIME type here because we check the MIME type in
  // ModuleScriptLoader::WasModuleLoadSuccessful().
  return true;
}

void ModuleScript::RunScript(LocalFrame* frame, const SecurityOrigin*) const {
  // TODO(hiroshige): Implement this once Modulator::ExecuteModule() is landed.
  NOTREACHED();
}

String ModuleScript::InlineSourceTextForCSP() const {
  // Currently we don't support inline module scripts.
  // TODO(hiroshige): Implement this.
  NOTREACHED();
  return String();
}

}  // namespace blink
