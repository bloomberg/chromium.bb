// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ModulatorImplBase_h
#define ModulatorImplBase_h

#include "bindings/core/v8/ScriptModule.h"
#include "core/dom/Modulator.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/TraceWrapperMember.h"
#include "platform/bindings/V8PerIsolateData.h"
#include "platform/heap/Handle.h"

namespace blink {

class DynamicModuleResolver;
class ExecutionContext;
class ModuleMap;
class ModuleScriptLoaderRegistry;
class ModuleTreeLinkerRegistry;
class ScriptState;
class WebTaskRunner;

// ModulatorImplBase is the base implementation of Modulator interface, which
// represents "environment settings object" concept for module scripts.
// ModulatorImplBase serves as the backplane for tieing all ES6 module algorithm
// components together.
class ModulatorImplBase : public Modulator {
 public:
  virtual ~ModulatorImplBase();
  void Trace(blink::Visitor*);
  virtual void TraceWrappers(const ScriptWrappableVisitor*) const;

  ExecutionContext* GetExecutionContext() const;

 protected:
  explicit ModulatorImplBase(scoped_refptr<ScriptState>);

  ScriptState* GetScriptState() override { return script_state_.get(); }

 private:
  // Implements Modulator

  ScriptModuleResolver* GetScriptModuleResolver() override {
    return script_module_resolver_.Get();
  }
  WebTaskRunner* TaskRunner() override { return task_runner_.get(); }
  ReferrerPolicy GetReferrerPolicy() override;
  const SecurityOrigin* GetSecurityOriginForFetch() override;

  void FetchTree(const ModuleScriptFetchRequest&, ModuleTreeClient*) override;
  void FetchDescendantsForInlineScript(ModuleScript*,
                                       ModuleTreeClient*) override;
  void FetchSingle(const ModuleScriptFetchRequest&,
                   ModuleGraphLevel,
                   SingleModuleClient*) override;
  ModuleScript* GetFetchedModuleScript(const KURL&) override;
  bool HasValidContext() override;
  void FetchNewSingleModule(const ModuleScriptFetchRequest&,
                            ModuleGraphLevel,
                            ModuleScriptLoaderClient*) override;
  void ResolveDynamically(const String& specifier,
                          const KURL&,
                          const ReferrerScriptInfo&,
                          ScriptPromiseResolver*) override;
  ModuleImportMeta HostGetImportMetaProperties(ScriptModule) const override;
  ScriptModule CompileModule(const String& script,
                             const String& url_str,
                             const ScriptFetchOptions&,
                             AccessControlStatus,
                             const TextPosition&,
                             ExceptionState&) override;
  ScriptValue InstantiateModule(ScriptModule) override;
  Vector<ModuleRequest> ModuleRequestsFromScriptModule(ScriptModule) override;
  ScriptValue ExecuteModule(const ModuleScript*, CaptureEvalErrorFlag) override;

  scoped_refptr<ScriptState> script_state_;
  scoped_refptr<WebTaskRunner> task_runner_;
  TraceWrapperMember<ModuleMap> map_;
  Member<ModuleScriptLoaderRegistry> loader_registry_;
  TraceWrapperMember<ModuleTreeLinkerRegistry> tree_linker_registry_;
  Member<ScriptModuleResolver> script_module_resolver_;
  Member<DynamicModuleResolver> dynamic_module_resolver_;
};

}  // namespace blink

#endif
