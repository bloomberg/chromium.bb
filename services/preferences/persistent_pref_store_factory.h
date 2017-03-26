// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_PERSISTENT_PREF_STORE_FACTORY_H_
#define SERVICES_PREFERENCES_PERSISTENT_PREF_STORE_FACTORY_H_

#include <memory>

#include "services/preferences/public/interfaces/preferences.mojom.h"

namespace base {
class SequencedWorkerPool;
}

namespace prefs {
class PersistentPrefStoreImpl;

// Create a new mojom::PersistentPrefStore impl that runs on |worker_pool|
// configured by |configuration|.
std::unique_ptr<PersistentPrefStoreImpl> CreatePersistentPrefStore(
    mojom::PersistentPrefStoreConfigurationPtr configuration,
    base::SequencedWorkerPool* worker_pool,
    base::Closure on_initialized);

}  // namespace prefs

#endif  // SERVICES_PREFERENCES_PERSISTENT_PREF_STORE_FACTORY_H_
