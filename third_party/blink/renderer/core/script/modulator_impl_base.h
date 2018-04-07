// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_MODULATOR_IMPL_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_MODULATOR_IMPL_BASE_H_

#include "base/single_thread_task_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/script_module.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_member.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class DynamicModuleResolver;
class ExecutionContext;
class ModuleMap;
class ModuleScriptLoaderRegistry;
class ModuleTreeLinkerRegistry;
class ScriptState;

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
  base::SingleThreadTaskRunner* TaskRunner() override {
    return task_runner_.get();
  }
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
                             const KURL& source_url,
                             const KURL& base_url,
                             const ScriptFetchOptions&,
                             AccessControlStatus,
                             const TextPosition&,
                             ExceptionState&) override;
  ScriptValue InstantiateModule(ScriptModule) override;
  Vector<ModuleRequest> ModuleRequestsFromScriptModule(ScriptModule) override;
  ScriptValue ExecuteModule(const ModuleScript*, CaptureEvalErrorFlag) override;

  // Populates |reason| and returns true if the dynamic import is disallowed on
  // the associated execution context. In that case, a caller of this function
  // is expected to reject the dynamic import with |reason|. If the dynamic
  // import is allowed on the execution context, returns false without
  // modification of |reason|.
  virtual bool IsDynamicImportForbidden(String* reason) = 0;

  scoped_refptr<ScriptState> script_state_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  TraceWrapperMember<ModuleMap> map_;
  Member<ModuleScriptLoaderRegistry> loader_registry_;
  TraceWrapperMember<ModuleTreeLinkerRegistry> tree_linker_registry_;
  Member<ScriptModuleResolver> script_module_resolver_;
  Member<DynamicModuleResolver> dynamic_module_resolver_;
};

}  // namespace blink

#endif
