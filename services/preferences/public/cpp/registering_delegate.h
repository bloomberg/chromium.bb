// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/memory/ref_counted.h"
#include "components/prefs/pref_value_store.h"
#include "services/preferences/public/interfaces/preferences.mojom.h"

class PrefStore;
class PrefNotifier;

namespace prefs {
class PrefStoreImpl;
}

namespace sync_preferences {

// Registers all provided |PrefStore|s with the pref service. The pref stores
// remaining registered for the life time of |this|.
class RegisteringDelegate : public PrefValueStore::Delegate {
 public:
  RegisteringDelegate(prefs::mojom::PrefStoreRegistryPtr registry);
  ~RegisteringDelegate() override;

  // PrefValueStore::Delegate:
  void Init(PrefStore* managed_prefs,
            PrefStore* supervised_user_prefs,
            PrefStore* extension_prefs,
            PrefStore* command_line_prefs,
            PrefStore* user_prefs,
            PrefStore* recommended_prefs,
            PrefStore* default_prefs,
            PrefNotifier* pref_notifier) override;
  void UpdateCommandLinePrefStore(PrefStore* command_line_prefs) override;

 private:
  // Expose the |backing_pref_store| through the prefs service.
  void RegisterPrefStore(scoped_refptr<PrefStore> backing_pref_store,
                         PrefValueStore::PrefStoreType type);

  prefs::mojom::PrefStoreRegistryPtr registry_;
  std::vector<std::unique_ptr<prefs::PrefStoreImpl>> impls_;
};

}  // namespace sync_preferences
