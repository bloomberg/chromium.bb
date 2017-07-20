// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ModuleTreeLinkerRegistry_h
#define ModuleTreeLinkerRegistry_h

#include "core/CoreExport.h"
#include "core/dom/AncestorList.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/TraceWrapperMember.h"
#include "platform/heap/Handle.h"

namespace blink {

class Modulator;
class ModuleScriptFetchRequest;
class ModuleTreeClient;
class ModuleTreeLinker;
enum class ModuleGraphLevel;
class ModuleScript;
class ModuleTreeReachedUrlSet;

// ModuleTreeLinkerRegistry keeps active ModuleTreeLinkers alive.
class CORE_EXPORT ModuleTreeLinkerRegistry
    : public GarbageCollected<ModuleTreeLinkerRegistry>,
      public TraceWrapperBase {
 public:
  static ModuleTreeLinkerRegistry* Create() {
    return new ModuleTreeLinkerRegistry;
  }
  DECLARE_TRACE();
  DECLARE_TRACE_WRAPPERS();

  ModuleTreeLinker* Fetch(const ModuleScriptFetchRequest&,
                          const AncestorList&,
                          ModuleGraphLevel,
                          Modulator*,
                          ModuleTreeReachedUrlSet*,
                          ModuleTreeClient*);
  ModuleTreeLinker* FetchDescendantsForInlineScript(ModuleScript*,
                                                    Modulator*,
                                                    ModuleTreeClient*);

 private:
  ModuleTreeLinkerRegistry() = default;

  friend class ModuleTreeLinker;
  void ReleaseFinishedFetcher(ModuleTreeLinker*);

  HeapHashSet<TraceWrapperMember<ModuleTreeLinker>> active_tree_linkers_;
};

}  // namespace blink

#endif
