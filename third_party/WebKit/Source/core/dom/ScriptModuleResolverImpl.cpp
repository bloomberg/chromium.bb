// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ScriptModuleResolverImpl.h"

#include "bindings/core/v8/ScriptModule.h"
#include "core/dom/Modulator.h"
#include "core/dom/ModuleScript.h"

namespace blink {

void ScriptModuleResolverImpl::RegisterModuleScript(
    ModuleScript* module_script) {
  DVLOG(1) << "ScriptModuleResolverImpl::registerModuleScript(url=\""
           << module_script->BaseURL().GetString()
           << "\", hash=" << ScriptModuleHash::GetHash(module_script->Record())
           << ")";

  DCHECK(module_script);
  DCHECK(!module_script->Record().IsNull());
  auto result =
      record_to_module_script_map_.Set(module_script->Record(), module_script);
  DCHECK(result.is_new_entry);
}

ScriptModule ScriptModuleResolverImpl::Resolve(
    const String& specifier,
    const ScriptModule& referrer,
    ExceptionState& exception_state) {
  // https://html.spec.whatwg.org/commit-snapshots/f8e75a974ed9185e5b462bc5b2dfb32034bd1145#hostresolveimportedmodule(referencingmodule,-specifier)
  DVLOG(1) << "ScriptModuleResolverImpl::resolve(specifier=\"" << specifier
           << ", referrer.hash=" << ScriptModuleHash::GetHash(referrer) << ")";

  // Step 1. Let referencing module script be referencingModule.[[HostDefined]].
  const auto it = record_to_module_script_map_.Find(referrer);
  CHECK_NE(it, record_to_module_script_map_.end())
      << "Failed to find referrer ModuleScript corresponding to the record";
  ModuleScript* referrer_module = it->value;

  // Step 2. Let moduleMap be referencing module script's settings object's
  // module map. Note: Blink finds out "module script's settings object"
  // (Modulator) from context where HostResolveImportedModule was called.

  // Step 3. Let url be the result of resolving a module specifier given
  // referencing module script and specifier. If the result is failure, then
  // throw a TypeError exception and abort these steps.
  KURL url =
      Modulator::ResolveModuleSpecifier(specifier, referrer_module->BaseURL());
  if (!url.IsValid()) {
    exception_state.ThrowTypeError("Failed to resolve module specifier '" +
                                   specifier + "'");
    return ScriptModule();
  }

  // Step 4. Let resolved module script be moduleMap[url]. If no such entry
  // exists, or if resolved module script is null or "fetching", then throw a
  // TypeError exception and abort these steps.
  ModuleScript* module_script = modulator_->GetFetchedModuleScript(url);
  if (!module_script) {
    exception_state.ThrowTypeError(
        "Failed to load module script for module specifier '" + specifier +
        "'");
    return ScriptModule();
  }

  // Step 5. If resolved module script's instantiation state is "errored", then
  // throw resolved module script's instantiation error.
  if (module_script->InstantiationState() ==
      ModuleInstantiationState::kErrored) {
    // TODO(kouhei): Implement this path once
    // https://codereview.chromium.org/2782403002/ landed.
    NOTIMPLEMENTED();
    return ScriptModule();
  }

  // Step 6. Assert: resolved module script's instantiation state is
  // "instantiated" (and thus its module record is not null).
  // TODO(kouhei): Enable below check once once
  // https://codereview.chromium.org/2782403002/ landed.
  // CHECK_EQ(moduleScript->instantiationState(),
  // ModuleInstantiationState::Instantiated);

  // Step 7. Return resolved module script's module record.
  return module_script->Record();
}

DEFINE_TRACE(ScriptModuleResolverImpl) {
  ScriptModuleResolver::Trace(visitor);
  visitor->Trace(record_to_module_script_map_);
  visitor->Trace(modulator_);
}

}  // namespace blink
