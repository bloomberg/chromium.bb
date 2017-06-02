// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/persistent_pref_store_factory.h"

#include <memory>
#include <utility>

#include "components/prefs/in_memory_pref_store.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_filter.h"
#include "services/preferences/persistent_pref_store_impl.h"
#include "services/preferences/tracked/tracked_persistent_pref_store_factory.h"

namespace prefs {
namespace {

std::unique_ptr<PersistentPrefStoreImpl> CreateSimplePersistentPrefStore(
    mojom::SimplePersistentPrefStoreConfigurationPtr config,
    base::SequencedWorkerPool* worker_pool,
    base::Closure on_initialized) {
  return base::MakeUnique<PersistentPrefStoreImpl>(
      new JsonPrefStore(config->pref_filename,
                        JsonPrefStore::GetTaskRunnerForFile(
                            config->pref_filename.DirName(), worker_pool),
                        nullptr),
      std::move(on_initialized));
}

}  // namespace

std::unique_ptr<PersistentPrefStoreImpl> CreatePersistentPrefStore(
    mojom::PersistentPrefStoreConfigurationPtr configuration,
    base::SequencedWorkerPool* worker_pool,
    base::Closure on_initialized) {
  if (configuration->is_simple_configuration()) {
    return CreateSimplePersistentPrefStore(
        std::move(configuration->get_simple_configuration()), worker_pool,
        std::move(on_initialized));
  }
  if (configuration->is_tracked_configuration()) {
    return base::MakeUnique<PersistentPrefStoreImpl>(
        CreateTrackedPersistentPrefStore(
            std::move(configuration->get_tracked_configuration()), worker_pool),
        std::move(on_initialized));
  }
  if (configuration->is_incognito_configuration()) {
    return base::MakeUnique<PersistentPrefStoreImpl>(
        base::MakeRefCounted<InMemoryPrefStore>(), std::move(on_initialized));
  }
  NOTREACHED();
  return nullptr;
}

}  // namespace prefs
