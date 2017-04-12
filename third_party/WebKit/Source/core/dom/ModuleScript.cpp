// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ModuleScript.h"

namespace blink {

void ModuleScript::SetInstantiationError(v8::Isolate* isolate,
                                         v8::Local<v8::Value> error) {
  DCHECK_EQ(instantiation_state_, ModuleInstantiationState::kUninstantiated);
  instantiation_state_ = ModuleInstantiationState::kErrored;

  DCHECK(!error.IsEmpty());
  instantiation_error_.Set(isolate, error);
}

void ModuleScript::SetInstantiationSuccess() {
  DCHECK_EQ(instantiation_state_, ModuleInstantiationState::kUninstantiated);
  instantiation_state_ = ModuleInstantiationState::kInstantiated;
}

DEFINE_TRACE(ModuleScript) {
  Script::Trace(visitor);
}
DEFINE_TRACE_WRAPPERS(ModuleScript) {
  Script::TraceWrappers(visitor);
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
