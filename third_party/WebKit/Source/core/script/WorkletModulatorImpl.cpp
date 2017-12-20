// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/script/WorkletModulatorImpl.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/loader/modulescript/WorkletModuleScriptFetcher.h"
#include "core/workers/WorkletGlobalScope.h"
#include "platform/bindings/V8ThrowException.h"

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
  return new WorkletModuleScriptFetcher(
      global_scope->ModuleResponsesMapProxy());
}

void WorkletModulatorImpl::ResolveDynamically(const String&,
                                              const KURL&,
                                              const ReferrerScriptInfo&,
                                              ScriptPromiseResolver* resolver) {
  resolver->Reject(V8ThrowException::CreateTypeError(
      GetScriptState()->GetIsolate(),
      "import() is disallowed on WorkletGlobalScope."));
}

}  // namespace blink
