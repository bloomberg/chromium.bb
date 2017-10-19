// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ModuleScriptLoaderRegistry_h
#define ModuleScriptLoaderRegistry_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/HashSet.h"

namespace blink {

class Modulator;
class ModuleScriptFetchRequest;
class ModuleScriptLoader;
class ModuleScriptLoaderClient;
enum class ModuleGraphLevel;

// ModuleScriptLoaderRegistry keeps active ModuleLoaders alive.
class CORE_EXPORT ModuleScriptLoaderRegistry final
    : public GarbageCollected<ModuleScriptLoaderRegistry> {
 public:
  static ModuleScriptLoaderRegistry* Create() {
    return new ModuleScriptLoaderRegistry;
  }
  void Trace(blink::Visitor*);

  ModuleScriptLoader* Fetch(const ModuleScriptFetchRequest&,
                            ModuleGraphLevel,
                            Modulator*,
                            ModuleScriptLoaderClient*);

 private:
  ModuleScriptLoaderRegistry() = default;

  friend class ModuleScriptLoader;
  void ReleaseFinishedLoader(ModuleScriptLoader*);

  HeapHashSet<Member<ModuleScriptLoader>> active_loaders_;
};

}  // namespace blink

#endif
