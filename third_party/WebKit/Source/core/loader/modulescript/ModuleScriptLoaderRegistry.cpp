// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/modulescript/ModuleScriptLoaderRegistry.h"

#include "core/loader/modulescript/ModuleScriptLoader.h"

namespace blink {

DEFINE_TRACE(ModuleScriptLoaderRegistry) {
  visitor->trace(m_activeLoaders);
}

ModuleScriptLoader* ModuleScriptLoaderRegistry::fetch(
    const ModuleScriptFetchRequest& request,
    ModuleGraphLevel level,
    Modulator* modulator,
    ResourceFetcher* fetcher,
    ModuleScriptLoaderClient* client) {
  ModuleScriptLoader* loader =
      ModuleScriptLoader::create(modulator, this, client);
  DCHECK(loader->isInitialState());
  m_activeLoaders.insert(loader);
  loader->fetch(request, fetcher, level);
  return loader;
}

void ModuleScriptLoaderRegistry::releaseFinishedLoader(
    ModuleScriptLoader* loader) {
  DCHECK(loader->hasFinished());

  auto it = m_activeLoaders.find(loader);
  DCHECK_NE(it, m_activeLoaders.end());
  m_activeLoaders.erase(it);
}

}  // namespace blink
