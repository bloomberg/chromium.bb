// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_PUBLIC_CPP_PREF_SERVICE_MAIN_H_
#define SERVICES_PREFERENCES_PUBLIC_CPP_PREF_SERVICE_MAIN_H_

#include <memory>
#include <set>

#include "base/memory/ref_counted.h"
#include "components/prefs/pref_value_store.h"

namespace base {
class SequencedWorkerPool;
}

namespace service_manager {
class Service;
}

namespace prefs {

std::unique_ptr<service_manager::Service> CreatePrefService(
    std::set<PrefValueStore::PrefStoreType> expected_pref_stores,
    scoped_refptr<base::SequencedWorkerPool> worker_pool);

}  // namespace prefs

#endif  // SERVICES_PREFERENCES_PUBLIC_CPP_PREF_SERVICE_MAIN_H_
