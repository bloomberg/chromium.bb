// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/DocumentModulatorImpl.h"

#include "core/loader/modulescript/DocumentModuleScriptFetcher.h"

namespace blink {

ModulatorImplBase* DocumentModulatorImpl::Create(
    scoped_refptr<ScriptState> script_state,
    ResourceFetcher* resource_fetcher) {
  return new DocumentModulatorImpl(std::move(script_state), resource_fetcher);
}

ModuleScriptFetcher* DocumentModulatorImpl::CreateModuleScriptFetcher() {
  return new DocumentModuleScriptFetcher(fetcher_);
}

void DocumentModulatorImpl::Trace(blink::Visitor* visitor) {
  visitor->Trace(fetcher_);
  ModulatorImplBase::Trace(visitor);
}

DocumentModulatorImpl::DocumentModulatorImpl(
    scoped_refptr<ScriptState> script_state,
    ResourceFetcher* resource_fetcher)
    : ModulatorImplBase(std::move(script_state)), fetcher_(resource_fetcher) {
  DCHECK(fetcher_);
}

}  // namespace blink
