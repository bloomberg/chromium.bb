// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Modulator_h
#define Modulator_h

#include "base/single_thread_task_runner.h"
#include "bindings/core/v8/ScriptModule.h"
#include "core/CoreExport.h"
#include "core/script/ModuleImportMeta.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/V8PerContextData.h"
#include "platform/heap/Handle.h"
#include "platform/loader/fetch/AccessControlStatus.h"
#include "platform/loader/fetch/ScriptFetchOptions.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/ReferrerPolicy.h"
#include "platform/wtf/text/TextPosition.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class ExceptionState;
class ModuleScript;
class ModuleScriptFetchRequest;
class ModuleScriptFetcher;
class ModuleScriptLoaderClient;
class ReferrerScriptInfo;
class ScriptModuleResolver;
class ScriptPromiseResolver;
class ScriptState;
class ScriptValue;
class SecurityOrigin;

// A SingleModuleClient is notified when single module script node (node as in a
// module tree graph) load is complete and its corresponding entry is created in
// module map.
class CORE_EXPORT SingleModuleClient
    : public GarbageCollectedFinalized<SingleModuleClient>,
      public TraceWrapperBase {
 public:
  virtual ~SingleModuleClient() = default;
  virtual void Trace(blink::Visitor* visitor) {}
  void TraceWrappers(const ScriptWrappableVisitor*) const override {}

  virtual void NotifyModuleLoadFinished(ModuleScript*) = 0;
};

// A ModuleTreeClient is notified when a module script and its whole descendent
// tree load is complete.
class CORE_EXPORT ModuleTreeClient
    : public GarbageCollectedFinalized<ModuleTreeClient>,
      public TraceWrapperBase {
 public:
  virtual ~ModuleTreeClient() = default;
  virtual void Trace(blink::Visitor* visitor) {}
  void TraceWrappers(const ScriptWrappableVisitor*) const override {}

  virtual void NotifyModuleTreeLoadFinished(ModuleScript*) = 0;
};

// spec: "top-level module fetch flag"
// https://html.spec.whatwg.org/multipage/webappapis.html#fetching-scripts-is-top-level
enum class ModuleGraphLevel { kTopLevelModuleFetch, kDependentModuleFetch };

// A Modulator is an interface for "environment settings object" concept for
// module scripts.
// https://html.spec.whatwg.org/#environment-settings-object
//
// A Modulator also serves as an entry point for various module spec algorithms.
class CORE_EXPORT Modulator : public GarbageCollectedFinalized<Modulator>,
                              public V8PerContextData::Data,
                              public TraceWrapperBase {
  USING_GARBAGE_COLLECTED_MIXIN(Modulator);

 public:
  static Modulator* From(ScriptState*);
  virtual ~Modulator();

  static void SetModulator(ScriptState*, Modulator*);
  static void ClearModulator(ScriptState*);

  virtual void Trace(blink::Visitor* visitor) {}
  void TraceWrappers(const ScriptWrappableVisitor*) const override {}

  virtual ScriptModuleResolver* GetScriptModuleResolver() = 0;
  virtual base::SingleThreadTaskRunner* TaskRunner() = 0;
  virtual ReferrerPolicy GetReferrerPolicy() = 0;

  // Returns the security origin of the "fetch client settings object".
  // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-a-module-worker-script-tree
  // This should be called only from ModuleScriptLoader.
  virtual const SecurityOrigin* GetSecurityOriginForFetch() = 0;

  virtual ScriptState* GetScriptState() = 0;

  // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-a-module-script-tree
  virtual void FetchTree(const ModuleScriptFetchRequest&,
                         ModuleTreeClient*) = 0;

  // Asynchronously retrieve a module script from the module map, or fetch it
  // and put it in the map if it's not there already.
  // https://html.spec.whatwg.org/#fetch-a-single-module-script
  virtual void FetchSingle(const ModuleScriptFetchRequest&,
                           ModuleGraphLevel,
                           SingleModuleClient*) = 0;

  virtual void FetchDescendantsForInlineScript(ModuleScript*,
                                               ModuleTreeClient*) = 0;

  // Synchronously retrieves a single module script from existing module map
  // entry.
  // Note: returns nullptr if the module map entry doesn't exist, or
  // is still "fetching".
  virtual ModuleScript* GetFetchedModuleScript(const KURL&) = 0;

  // https://html.spec.whatwg.org/#resolve-a-module-specifier
  static KURL ResolveModuleSpecifier(const String& module_request,
                                     const KURL& base_url,
                                     String* failure_reason = nullptr);

  // https://tc39.github.io/proposal-dynamic-import/#sec-hostimportmoduledynamically
  virtual void ResolveDynamically(const String& specifier,
                                  const KURL&,
                                  const ReferrerScriptInfo&,
                                  ScriptPromiseResolver*) = 0;

  // https://html.spec.whatwg.org/#hostgetimportmetaproperties
  virtual ModuleImportMeta HostGetImportMetaProperties(ScriptModule) const = 0;

  virtual bool HasValidContext() = 0;

  virtual ScriptModule CompileModule(const String& script,
                                     const KURL& source_url,
                                     const KURL& base_url,
                                     const ScriptFetchOptions&,
                                     AccessControlStatus,
                                     const TextPosition&,
                                     ExceptionState&) = 0;

  virtual ScriptValue InstantiateModule(ScriptModule) = 0;

  struct ModuleRequest {
    String specifier;
    TextPosition position;
    ModuleRequest(const String& specifier, const TextPosition& position)
        : specifier(specifier), position(position) {}
  };
  virtual Vector<ModuleRequest> ModuleRequestsFromScriptModule(
      ScriptModule) = 0;

  enum class CaptureEvalErrorFlag : bool { kReport, kCapture };

  // ExecuteModule implements #run-a-module-script HTML spec algorithm.
  // https://html.spec.whatwg.org/multipage/webappapis.html#run-a-module-script
  // CaptureEvalErrorFlag is used to implement "rethrow errors" parameter in
  // run-a-module-script.
  // - When "rethrow errors" is to be set, use kCapture for EvaluateModule().
  // Then EvaluateModule() returns an exception if any (instead of throwing it),
  // and the caller should rethrow the returned exception. - When "rethrow
  // errors" is not to be set, use kReport. EvaluateModule() "report the error"
  // inside it (if any), and always returns null ScriptValue().
  virtual ScriptValue ExecuteModule(const ModuleScript*,
                                    CaptureEvalErrorFlag) = 0;

  virtual ModuleScriptFetcher* CreateModuleScriptFetcher() = 0;

 private:
  friend class ModuleMap;

  // Fetches a single module script.
  // This is triggered from fetchSingle() implementation (which is in ModuleMap)
  // if the cached entry doesn't exist.
  // The client can be notified either synchronously or asynchronously.
  virtual void FetchNewSingleModule(const ModuleScriptFetchRequest&,
                                    ModuleGraphLevel,
                                    ModuleScriptLoaderClient*) = 0;
};

}  // namespace blink

#endif
