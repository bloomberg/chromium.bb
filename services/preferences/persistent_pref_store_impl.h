// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_PERSISTENT_PREF_STORE_IMPL_H_
#define SERVICES_PREFERENCES_PERSISTENT_PREF_STORE_IMPL_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/macros.h"
#include "services/preferences/public/interfaces/preferences.mojom.h"
#include "services/preferences/public/interfaces/tracked_preference_validation_delegate.mojom.h"

namespace base {
class Value;
}

namespace prefs {

class PersistentPrefStoreImpl : public PrefStore::Observer {
 public:
  // If |initialized()| is false after construction, |on_initialized| will be
  // called when it becomes true.
  PersistentPrefStoreImpl(
      scoped_refptr<PersistentPrefStore> backing_pref_store,
      mojom::TrackedPreferenceValidationDelegatePtr validation_delegate,
      base::OnceClosure on_initialized);

  ~PersistentPrefStoreImpl() override;

  mojom::PersistentPrefStoreConnectionPtr CreateConnection();

  bool initialized() { return !initializing_; }

 private:
  class Connection;

  void SetValue(const std::string& key,
                std::unique_ptr<base::Value> value,
                uint32_t flags);

  void CommitPendingWrite();
  void SchedulePendingLossyWrites();
  void ClearMutableValues();

  // PrefStore::Observer:
  void OnPrefValueChanged(const std::string& key) override;
  void OnInitializationCompleted(bool succeeded) override;

  void OnConnectionError(Connection* connection);

  scoped_refptr<PersistentPrefStore> backing_pref_store_;
  mojom::TrackedPreferenceValidationDelegatePtr validation_delegate_;

  bool initializing_ = false;

  std::unordered_map<Connection*, std::unique_ptr<Connection>> connections_;

  base::OnceClosure on_initialized_;

  DISALLOW_COPY_AND_ASSIGN(PersistentPrefStoreImpl);
};

}  // namespace prefs

#endif  // SERVICES_PREFERENCES_PERSISTENT_PREF_STORE_IMPL_H_
