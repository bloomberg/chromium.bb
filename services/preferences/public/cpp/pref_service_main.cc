// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/public/cpp/pref_service_main.h"

#include "base/threading/sequenced_worker_pool.h"
#include "services/preferences/pref_store_manager_impl.h"
#include "services/service_manager/public/cpp/service.h"

namespace prefs {

std::unique_ptr<service_manager::Service> CreatePrefService(
    std::set<PrefValueStore::PrefStoreType> expected_pref_stores,
    scoped_refptr<base::SequencedWorkerPool> worker_pool) {
  return base::MakeUnique<PrefStoreManagerImpl>(expected_pref_stores,
                                                std::move(worker_pool));
}

}  // namespace prefs
