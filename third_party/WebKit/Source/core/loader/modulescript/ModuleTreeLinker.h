// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ModuleTreeLinker_h
#define ModuleTreeLinker_h

#include "core/CoreExport.h"
#include "core/dom/AncestorList.h"
#include "core/dom/Modulator.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/TraceWrapperMember.h"

namespace blink {

class ModuleScriptFetchRequest;
class ModuleTreeLinkerRegistry;
class ModuleTreeReachedUrlSet;

// A ModuleTreeLinker is responsible for running and keeping intermediate states
// for "internal module script graph fetching procedure" for a module graph tree
// node.
// https://html.spec.whatwg.org/multipage/webappapis.html#internal-module-script-graph-fetching-procedure
class CORE_EXPORT ModuleTreeLinker final : public SingleModuleClient {
 public:
  static ModuleTreeLinker* Fetch(const ModuleScriptFetchRequest&,
                                 const AncestorList&,
                                 ModuleGraphLevel,
                                 Modulator*,
                                 ModuleTreeReachedUrlSet*,
                                 ModuleTreeLinkerRegistry*,
                                 ModuleTreeClient*);
  static ModuleTreeLinker* FetchDescendantsForInlineScript(
      ModuleScript*,
      Modulator*,
      ModuleTreeLinkerRegistry*,
      ModuleTreeClient*);

  virtual ~ModuleTreeLinker() = default;
  DECLARE_TRACE();
  DECLARE_TRACE_WRAPPERS();

  bool IsFetching() const {
    return State::kFetchingSelf <= state_ && state_ < State::kFinished;
  }
  bool HasFinished() const { return state_ == State::kFinished; }

 private:
  ModuleTreeLinker(const AncestorList& ancestor_list_with_url,
                   ModuleGraphLevel,
                   Modulator*,
                   ModuleTreeReachedUrlSet*,
                   ModuleTreeLinkerRegistry*,
                   ModuleTreeClient*);

  enum class State {
    kInitial,
    // Running fetch of the module script corresponding to the target node.
    kFetchingSelf,
    // Running fetch of descendants of the target node.
    kFetchingDependencies,
    // Instantiating module_script_ and the node descendants.
    kInstantiating,
    kFinished,
  };
#if DCHECK_IS_ON()
  static const char* StateToString(State);
#endif
  void AdvanceState(State);

  void FetchSelf(const ModuleScriptFetchRequest&);
  // Implements SingleModuleClient
  void NotifyModuleLoadFinished(ModuleScript*) override;

  void FetchDescendants();
  void NotifyOneDescendantFinished();

  void Instantiate();

  class DependencyModuleClient;
  friend class DependencyModuleClient;

  const Member<Modulator> modulator_;
  const Member<ModuleTreeReachedUrlSet> reached_url_set_;
  const Member<ModuleTreeLinkerRegistry> registry_;
  const Member<ModuleTreeClient> client_;
  const HashSet<KURL> ancestor_list_with_url_;
  const ModuleGraphLevel level_;
  State state_ = State::kInitial;
  // Correspond to _result_ in
  // https://html.spec.whatwg.org/multipage/webappapis.html#internal-module-script-graph-fetching-procedure
  TraceWrapperMember<ModuleScript> module_script_;
  size_t num_incomplete_descendants_ = 0;
  HeapHashSet<Member<DependencyModuleClient>> dependency_clients_;
};

}  // namespace blink

#endif
