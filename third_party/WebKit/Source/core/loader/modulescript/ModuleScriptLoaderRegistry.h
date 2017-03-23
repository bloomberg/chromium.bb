// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ModuleScriptLoaderRegistry_h
#define ModuleScriptLoaderRegistry_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "wtf/HashSet.h"

namespace blink {

class Modulator;
class ModuleScriptFetchRequest;
class ModuleScriptLoader;
class ModuleScriptLoaderClient;
class ResourceFetcher;
enum class ModuleGraphLevel;

// ModuleScriptLoaderRegistry keeps active ModuleLoaders alive.
class CORE_EXPORT ModuleScriptLoaderRegistry final
    : public GarbageCollected<ModuleScriptLoaderRegistry> {
 public:
  static ModuleScriptLoaderRegistry* create() {
    return new ModuleScriptLoaderRegistry;
  }
  DECLARE_TRACE();

  ModuleScriptLoader* fetch(const ModuleScriptFetchRequest&,
                            ModuleGraphLevel,
                            Modulator*,
                            ResourceFetcher*,
                            ModuleScriptLoaderClient*);

 private:
  ModuleScriptLoaderRegistry() = default;

  friend class ModuleScriptLoader;
  void releaseFinishedLoader(ModuleScriptLoader*);

  HeapHashSet<Member<ModuleScriptLoader>> m_activeLoaders;
};

}  // namespace blink

#endif
