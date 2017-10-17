// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/WorkletModulatorImpl.h"

#include "core/loader/modulescript/WorkletModuleScriptFetcher.h"
#include "core/workers/WorkletGlobalScope.h"

namespace blink {

ModulatorImplBase* WorkletModulatorImpl::Create(
    RefPtr<ScriptState> script_state) {
  return new WorkletModulatorImpl(std::move(script_state));
}

WorkletModulatorImpl::WorkletModulatorImpl(RefPtr<ScriptState> script_state)
    : ModulatorImplBase(std::move(script_state)) {}

SecurityOrigin* WorkletModulatorImpl::GetSecurityOriginForFetch() {
  return ToWorkletGlobalScope(GetExecutionContext())->DocumentSecurityOrigin();
}

ModuleScriptFetcher* WorkletModulatorImpl::CreateModuleScriptFetcher() {
  auto global_scope = ToWorkletGlobalScope(GetExecutionContext());
  return new WorkletModuleScriptFetcher(
      global_scope->ModuleResponsesMapProxy());
}

}  // namespace blink
