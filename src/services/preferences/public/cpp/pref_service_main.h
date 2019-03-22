// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_PUBLIC_CPP_PREF_SERVICE_MAIN_H_
#define SERVICES_PREFERENCES_PUBLIC_CPP_PREF_SERVICE_MAIN_H_

#include <memory>
#include <set>

#include "base/memory/ref_counted.h"
#include "components/prefs/pref_value_store.h"
#include "services/service_manager/public/mojom/service.mojom.h"

class PersistentPrefStore;
class PrefRegistry;

namespace service_manager {
class Service;
}

namespace prefs {

// Creates a PrefService and a closure that can be used to shut it down.
//
// TODO(https://crbug.com/908649): This should NOT be in the public client
// library for the prefs service. Neither should anything here which depends on
// it, like InProcessPrefServiceFactory.
std::pair<std::unique_ptr<service_manager::Service>, base::OnceClosure>
CreatePrefService(service_manager::mojom::ServiceRequest request,
                  PrefStore* managed_prefs,
                  PrefStore* supervised_user_prefs,
                  PrefStore* extension_prefs,
                  PrefStore* command_line_prefs,
                  PersistentPrefStore* user_prefs,
                  PersistentPrefStore* incognito_user_prefs_underlay,
                  PrefStore* recommended_prefs,
                  PrefRegistry* pref_registry,
                  std::vector<const char*> overlay_pref_names);

}  // namespace prefs

#endif  // SERVICES_PREFERENCES_PUBLIC_CPP_PREF_SERVICE_MAIN_H_
