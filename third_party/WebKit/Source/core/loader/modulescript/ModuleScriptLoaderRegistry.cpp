// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/modulescript/ModuleScriptLoaderRegistry.h"

#include "core/loader/modulescript/ModuleScriptLoader.h"

namespace blink {

DEFINE_TRACE(ModuleScriptLoaderRegistry) {
  visitor->Trace(active_loaders_);
}

ModuleScriptLoader* ModuleScriptLoaderRegistry::Fetch(
    const ModuleScriptFetchRequest& request,
    ModuleGraphLevel level,
    Modulator* modulator,
    ResourceFetcher* fetcher,
    ModuleScriptLoaderClient* client) {
  ModuleScriptLoader* loader =
      ModuleScriptLoader::Create(modulator, this, client);
  DCHECK(loader->IsInitialState());
  active_loaders_.insert(loader);
  loader->Fetch(request, fetcher, level);
  return loader;
}

void ModuleScriptLoaderRegistry::ReleaseFinishedLoader(
    ModuleScriptLoader* loader) {
  DCHECK(loader->HasFinished());

  auto it = active_loaders_.Find(loader);
  DCHECK_NE(it, active_loaders_.end());
  active_loaders_.erase(it);
}

}  // namespace blink
