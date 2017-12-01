// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/WorkerModulatorImpl.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "platform/bindings/V8ThrowException.h"

namespace blink {

ModulatorImplBase* WorkerModulatorImpl::Create(
    scoped_refptr<ScriptState> script_state) {
  return new WorkerModulatorImpl(std::move(script_state));
}

WorkerModulatorImpl::WorkerModulatorImpl(
    scoped_refptr<ScriptState> script_state)
    : ModulatorImplBase(std::move(script_state)) {}

ModuleScriptFetcher* WorkerModulatorImpl::CreateModuleScriptFetcher() {
  // TODO(nhiroki): Support module loading for workers.
  // (https://crbug.com/680046)
  NOTIMPLEMENTED();
  return nullptr;
}

void WorkerModulatorImpl::ResolveDynamically(const String& specifier,
                                             const KURL&,
                                             const ReferrerScriptInfo&,
                                             ScriptPromiseResolver* resolver) {
  // TODO(nhiroki): Support module loading for workers.
  // (https://crbug.com/680046)
  resolver->Reject(V8ThrowException::CreateTypeError(
      GetScriptState()->GetIsolate(),
      "Module scripts are not supported on WorkerGlobalScope yet (see "
      "https://crbug.com/680046)."));
}

}  // namespace blink
