// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/script/WorkletModulatorImpl.h"

#include "core/loader/modulescript/WorkerOrWorkletModuleScriptFetcher.h"
#include "core/workers/WorkletGlobalScope.h"

namespace blink {

ModulatorImplBase* WorkletModulatorImpl::Create(
    scoped_refptr<ScriptState> script_state) {
  return new WorkletModulatorImpl(std::move(script_state));
}

WorkletModulatorImpl::WorkletModulatorImpl(
    scoped_refptr<ScriptState> script_state)
    : ModulatorImplBase(std::move(script_state)) {}

const SecurityOrigin* WorkletModulatorImpl::GetSecurityOriginForFetch() {
  return ToWorkletGlobalScope(GetExecutionContext())->DocumentSecurityOrigin();
}

ModuleScriptFetcher* WorkletModulatorImpl::CreateModuleScriptFetcher() {
  auto global_scope = ToWorkletGlobalScope(GetExecutionContext());
  return new WorkerOrWorkletModuleScriptFetcher(
      global_scope->ModuleFetchCoordinatorProxy());
}

bool WorkletModulatorImpl::IsDynamicImportForbidden(String* reason) {
  *reason = "import() is disallowed on WorkletGlobalScope.";
  return true;
}

}  // namespace blink
