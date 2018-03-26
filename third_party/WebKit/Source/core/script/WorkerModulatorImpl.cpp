// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/script/WorkerModulatorImpl.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/loader/modulescript/WorkerOrWorkletModuleScriptFetcher.h"
#include "core/workers/WorkerGlobalScope.h"
#include "platform/bindings/V8ThrowException.h"
#include "platform/runtime_enabled_features.h"

namespace blink {

ModulatorImplBase* WorkerModulatorImpl::Create(
    scoped_refptr<ScriptState> script_state) {
  return new WorkerModulatorImpl(std::move(script_state));
}

WorkerModulatorImpl::WorkerModulatorImpl(
    scoped_refptr<ScriptState> script_state)
    : ModulatorImplBase(std::move(script_state)) {}

ModuleScriptFetcher* WorkerModulatorImpl::CreateModuleScriptFetcher() {
  auto* global_scope = ToWorkerGlobalScope(GetExecutionContext());
  return new WorkerOrWorkletModuleScriptFetcher(
      global_scope->ModuleFetchCoordinatorProxy());
}

bool WorkerModulatorImpl::IsDynamicImportForbidden(String* reason) {
  // TODO(nhiroki): Remove this flag check once module loading for
  // DedicatedWorker is enabled by default (https://crbug.com/680046).
  if (GetExecutionContext()->IsDedicatedWorkerGlobalScope() &&
      RuntimeEnabledFeatures::ModuleDedicatedWorkerEnabled()) {
    return false;
  }

  // TODO(nhiroki): Support module loading for SharedWorker and Service Worker.
  // (https://crbug.com/680046)
  *reason =
      "Module scripts are not supported on WorkerGlobalScope yet (see "
      "https://crbug.com/680046).";
  return true;
}

}  // namespace blink
