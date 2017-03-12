// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/persistent_pref_store_factory.h"

#include <memory>
#include <utility>

#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_filter.h"
#include "services/preferences/persistent_pref_store_impl.h"

namespace prefs {
namespace {

std::unique_ptr<PersistentPrefStoreImpl> CreateSimplePersistentPrefStore(
    mojom::SimplePersistentPrefStoreConfigurationPtr config,
    base::SequencedWorkerPool* worker_pool) {
  return base::MakeUnique<PersistentPrefStoreImpl>(
      new JsonPrefStore(config->pref_filename,
                        JsonPrefStore::GetTaskRunnerForFile(
                            config->pref_filename.DirName(), worker_pool),
                        nullptr),
      nullptr);
}

}  // namespace

std::unique_ptr<PersistentPrefStoreImpl> CreatePersistentPrefStore(
    mojom::PersistentPrefStoreConfigurationPtr configuration,
    base::SequencedWorkerPool* worker_pool) {
  if (configuration->is_simple_configuration()) {
    return CreateSimplePersistentPrefStore(
        std::move(configuration->get_simple_configuration()), worker_pool);
  }
  NOTREACHED();
  return nullptr;
}

}  // namespace prefs
