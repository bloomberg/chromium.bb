// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ModuleTreeLinkerRegistry_h
#define ModuleTreeLinkerRegistry_h

#include "core/CoreExport.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/TraceWrapperMember.h"
#include "platform/heap/Handle.h"

namespace blink {

class Modulator;
class ModuleScriptFetchRequest;
class ModuleTreeClient;
class ModuleTreeLinker;
class ModuleScript;

// ModuleTreeLinkerRegistry keeps active ModuleTreeLinkers alive.
class CORE_EXPORT ModuleTreeLinkerRegistry
    : public GarbageCollected<ModuleTreeLinkerRegistry>,
      public TraceWrapperBase {
 public:
  static ModuleTreeLinkerRegistry* Create() {
    return new ModuleTreeLinkerRegistry;
  }
  void Trace(blink::Visitor*);
  void TraceWrappers(const ScriptWrappableVisitor*) const;

  ModuleTreeLinker* Fetch(const ModuleScriptFetchRequest&,
                          Modulator*,
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
