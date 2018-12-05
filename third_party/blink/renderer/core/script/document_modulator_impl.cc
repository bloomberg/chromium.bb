// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/document_modulator_impl.h"

#include "third_party/blink/renderer/core/loader/modulescript/document_module_script_fetcher.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

ModulatorImplBase* DocumentModulatorImpl::Create(ScriptState* script_state) {
  return MakeGarbageCollected<DocumentModulatorImpl>(script_state);
}

DocumentModulatorImpl::DocumentModulatorImpl(ScriptState* script_state)
    : ModulatorImplBase(script_state) {}

ModuleScriptFetcher* DocumentModulatorImpl::CreateModuleScriptFetcher(
    ModuleScriptCustomFetchType custom_fetch_type) {
  DCHECK_EQ(ModuleScriptCustomFetchType::kNone, custom_fetch_type);
  return MakeGarbageCollected<DocumentModuleScriptFetcher>();
}

bool DocumentModulatorImpl::IsDynamicImportForbidden(String* reason) {
  return false;
}

}  // namespace blink
