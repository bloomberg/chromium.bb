// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/modulescript/ModuleTreeLinkerRegistry.h"

#include "core/loader/modulescript/ModuleTreeLinker.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/KURLHash.h"

namespace blink {

void ModuleTreeLinkerRegistry::Trace(blink::Visitor* visitor) {
  visitor->Trace(active_tree_linkers_);
}

DEFINE_TRACE_WRAPPERS(ModuleTreeLinkerRegistry) {
  for (const auto& member : active_tree_linkers_)
    visitor->TraceWrappers(member);
}

ModuleTreeLinker* ModuleTreeLinkerRegistry::Fetch(
    const ModuleScriptFetchRequest& request,
    Modulator* modulator,
    ModuleTreeClient* client) {
  ModuleTreeLinker* fetcher =
      ModuleTreeLinker::Fetch(request, modulator, this, client);
  DCHECK(fetcher->IsFetching());
  active_tree_linkers_.insert(fetcher);
  return fetcher;
}

ModuleTreeLinker* ModuleTreeLinkerRegistry::FetchDescendantsForInlineScript(
    ModuleScript* module_script,
    Modulator* modulator,
    ModuleTreeClient* client) {
  ModuleTreeLinker* fetcher = ModuleTreeLinker::FetchDescendantsForInlineScript(
      module_script, modulator, this, client);
  DCHECK(fetcher->IsFetching());
  active_tree_linkers_.insert(fetcher);
  return fetcher;
}

void ModuleTreeLinkerRegistry::ReleaseFinishedFetcher(
    ModuleTreeLinker* fetcher) {
  DCHECK(fetcher->HasFinished());

  auto it = active_tree_linkers_.find(fetcher);
  DCHECK_NE(it, active_tree_linkers_.end());
  active_tree_linkers_.erase(it);
}

}  // namespace blink
