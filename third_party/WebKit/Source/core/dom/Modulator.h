// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Modulator_h
#define Modulator_h

#include "bindings/core/v8/ScriptModule.h"
#include "core/CoreExport.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/V8PerContextData.h"
#include "platform/heap/Handle.h"
#include "platform/loader/fetch/AccessControlStatus.h"
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
class WebTaskRunner;

// A SingleModuleClient is notified when single module script node (node as in a
// module tree graph) load is complete and its corresponding entry is created in
// module map.
class CORE_EXPORT SingleModuleClient
    : public GarbageCollectedFinalized<SingleModuleClient>,
      public TraceWrapperBase {
 public:
  virtual ~SingleModuleClient() = default;
  DEFINE_INLINE_VIRTUAL_TRACE() {}

  virtual void NotifyModuleLoadFinished(ModuleScript*) = 0;
};

// A ModuleTreeClient is notified when a module script and its whole descendent
// tree load is complete.
class CORE_EXPORT ModuleTreeClient
    : public GarbageCollectedFinalized<ModuleTreeClient>,
      public TraceWrapperBase {
 public:
  virtual ~ModuleTreeClient() = default;
  DEFINE_INLINE_VIRTUAL_TRACE() {}

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

  DEFINE_INLINE_VIRTUAL_TRACE() {}

  virtual ScriptModuleResolver* GetScriptModuleResolver() = 0;
  virtual WebTaskRunner* TaskRunner() = 0;
  virtual ReferrerPolicy GetReferrerPolicy() = 0;

  // Returns the security origin of the "fetch client settings object".
  // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-a-module-worker-script-tree
  // This should be called only from ModuleScriptLoader.
  virtual SecurityOrigin* GetSecurityOriginForFetch() = 0;

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
                                     const KURL& base_url);

  // https://tc39.github.io/proposal-dynamic-import/#sec-hostimportmoduledynamically
  virtual void ResolveDynamically(const String& specifier,
                                  const KURL&,
                                  const ReferrerScriptInfo&,
                                  ScriptPromiseResolver*) = 0;

  virtual bool HasValidContext() = 0;

  virtual ScriptModule CompileModule(const String& script,
                                     const String& url_str,
                                     AccessControlStatus,
                                     WebURLRequest::FetchCredentialsMode,
                                     const String& nonce,
                                     ParserDisposition,
                                     const TextPosition&,
                                     ExceptionState&) = 0;

  virtual ScriptValue InstantiateModule(ScriptModule) = 0;

  virtual ScriptModuleState GetRecordStatus(ScriptModule) = 0;

  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-module-script-error
  virtual ScriptValue GetError(const ModuleScript*) = 0;

  struct ModuleRequest {
    String specifier;
    TextPosition position;
    ModuleRequest(const String& specifier, const TextPosition& position)
        : specifier(specifier), position(position) {}
  };
  virtual Vector<ModuleRequest> ModuleRequestsFromScriptModule(
      ScriptModule) = 0;

  // ExecuteModule implements #run-a-module-script HTML spec algorithm.
  // https://html.spec.whatwg.org/multipage/webappapis.html#run-a-module-script
  // Note: "rethrow errors" flag in the spec corresponds to capture_error being
  // CaptureEvalErrorFlag::kCapture. Here we rely on caller to handle the
  // exception. The current only caller is
  // DynamicImportTreeClinet::NotifyModuleTreeLoadFinished, which catches
  // the exception immediately, so just returning the exception value here is
  // more convenient and optimal.
  virtual ScriptValue ExecuteModule(
      const ModuleScript*,
      CaptureEvalErrorFlag = CaptureEvalErrorFlag::kReport) = 0;

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
