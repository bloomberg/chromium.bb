// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Modulator_h
#define Modulator_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"

namespace blink {

class LocalFrame;
class ModuleScript;
class ModuleScriptFetchRequest;
class ModuleScriptLoaderClient;
class ScriptModuleResolver;
class WebTaskRunner;

// A SingleModuleClient is notified when single module script node (node as in a
// module tree graph) load is complete and its corresponding entry is created in
// module map.
class SingleModuleClient : public GarbageCollectedMixin {
 public:
  virtual void notifyModuleLoadFinished(ModuleScript*) = 0;
};

// spec: "top-level module fetch flag"
// https://html.spec.whatwg.org/multipage/webappapis.html#fetching-scripts-is-top-level
enum class ModuleGraphLevel { TopLevelModuleFetch, DependentModuleFetch };

// A Modulator is an interface for "environment settings object" concept for
// module scripts.
// https://html.spec.whatwg.org/#environment-settings-object
//
// A Modulator also serves as an entry point for various module spec algorithms.
class CORE_EXPORT Modulator : public GarbageCollectedMixin {
 public:
  static Modulator* from(LocalFrame*);

  virtual ScriptModuleResolver* scriptModuleResolver() = 0;
  virtual WebTaskRunner* taskRunner() = 0;

  // https://html.spec.whatwg.org/#resolve-a-module-specifier
  static KURL resolveModuleSpecifier(const String& moduleRequest,
                                     const KURL& baseURL);

 private:
  friend class ModuleMap;

  // Fetches a single module script.
  // This is triggered from fetchSingle() implementation (which is in ModuleMap)
  // if the cached entry doesn't exist.
  // The client can be notified either synchronously or asynchronously.
  virtual void fetchNewSingleModule(const ModuleScriptFetchRequest&,
                                    ModuleGraphLevel,
                                    ModuleScriptLoaderClient*) = 0;
};

}  // namespace blink

#endif
