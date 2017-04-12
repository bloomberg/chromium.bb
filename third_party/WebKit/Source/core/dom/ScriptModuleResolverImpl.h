// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptModuleResolverImpl_h
#define ScriptModuleResolverImpl_h

#include "bindings/core/v8/ScriptModule.h"
#include "core/CoreExport.h"
#include "core/dom/ScriptModuleResolver.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class Modulator;
class ModuleScript;
class ScriptModule;

// The ScriptModuleResolverImpl implements ScriptModuleResolver interface
// and implements "HostResolveImportedModule" HTML spec algorithm to bridge
// ModuleMap (via Modulator) and V8 bindings.
class CORE_EXPORT ScriptModuleResolverImpl final : public ScriptModuleResolver {
 public:
  static ScriptModuleResolverImpl* Create(Modulator* modulator) {
    return new ScriptModuleResolverImpl(modulator);
  }

  DECLARE_VIRTUAL_TRACE();

  // Implements ScriptModuleResolver:

  void RegisterModuleScript(ModuleScript*) override;
  // Implements "Runtime Semantics: HostResolveImportedModule" per HTML spec.
  // https://html.spec.whatwg.org/#hostresolveimportedmodule(referencingmodule,-specifier)
  ScriptModule Resolve(const String& specifier,
                       const ScriptModule& referrer,
                       ExceptionState&) override;

 private:
  explicit ScriptModuleResolverImpl(Modulator* modulator)
      : modulator_(modulator) {}

  // Corresponds to the spec concept "referencingModule.[[HostDefined]]".
  HeapHashMap<ScriptModule, Member<ModuleScript>> record_to_module_script_map_;
  Member<Modulator> modulator_;
};

}  // namespace blink

#endif  // ScriptModuleResolverImpl_h
