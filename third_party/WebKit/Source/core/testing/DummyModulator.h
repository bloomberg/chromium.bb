// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DummyModulator_h
#define DummyModulator_h

#include "base/single_thread_task_runner.h"
#include "bindings/core/v8/ScriptModule.h"
#include "core/script/Modulator.h"
#include "platform/heap/Handle.h"

namespace blink {

class ModuleScriptLoaderClient;
class ScriptModuleResolver;
class ModuleScriptFetchRequest;

// DummyModulator provides empty Modulator interface implementation w/
// NOTREACHED().
//
// DummyModulator is useful for unit-testing.
// Not all module implementation components require full-blown Modulator
// implementation. Unit tests can implement a subset of Modulator interface
// which is exercised from unit-under-test.
class DummyModulator : public Modulator {
  DISALLOW_COPY_AND_ASSIGN(DummyModulator);

 public:
  DummyModulator();
  virtual ~DummyModulator();
  void Trace(blink::Visitor*);

  ScriptModuleResolver* GetScriptModuleResolver() override;
  base::SingleThreadTaskRunner* TaskRunner() override;
  ReferrerPolicy GetReferrerPolicy() override;
  const SecurityOrigin* GetSecurityOriginForFetch() override;
  ScriptState* GetScriptState() override;

  void FetchTree(const ModuleScriptFetchRequest&, ModuleTreeClient*) override;
  void FetchSingle(const ModuleScriptFetchRequest&,
                   ModuleGraphLevel,
                   SingleModuleClient*) override;
  void FetchDescendantsForInlineScript(ModuleScript*,
                                       ModuleTreeClient*) override;
  ModuleScript* GetFetchedModuleScript(const KURL&) override;
  void FetchNewSingleModule(const ModuleScriptFetchRequest&,
                            ModuleGraphLevel,
                            ModuleScriptLoaderClient*) override;
  bool HasValidContext() override;
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
  ModuleScriptFetcher* CreateModuleScriptFetcher() override;

  Member<ScriptModuleResolver> resolver_;
};

}  // namespace blink

#endif  // DummyModulator_h
