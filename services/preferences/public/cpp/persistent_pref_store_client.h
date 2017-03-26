// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_PUBLIC_CPP_PERSISTENT_PREF_STORE_CLIENT_H_
#define SERVICES_PREFERENCES_PUBLIC_CPP_PERSISTENT_PREF_STORE_CLIENT_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "components/prefs/persistent_pref_store.h"
#include "services/preferences/public/cpp/pref_store_client_mixin.h"
#include "services/preferences/public/interfaces/preferences.mojom.h"

namespace base {
class Value;
}

namespace prefs {

// An implementation of PersistentPrefStore backed by a
// mojom::PersistentPrefStore and a mojom::PrefStoreObserver.
class PersistentPrefStoreClient
    : public PrefStoreClientMixin<PersistentPrefStore> {
 public:
  explicit PersistentPrefStoreClient(mojom::PrefStoreConnectorPtr connector);

  explicit PersistentPrefStoreClient(
      mojom::PersistentPrefStoreConnectionPtr connection);

  // WriteablePrefStore:
  void SetValue(const std::string& key,
                std::unique_ptr<base::Value> value,
                uint32_t flags) override;
  void RemoveValue(const std::string& key, uint32_t flags) override;
  bool GetMutableValue(const std::string& key, base::Value** result) override;
  void ReportValueChanged(const std::string& key, uint32_t flags) override;
  void SetValueSilently(const std::string& key,
                        std::unique_ptr<base::Value> value,
                        uint32_t flags) override;

  // PersistentPrefStore:
  bool ReadOnly() const override;
  PrefReadError GetReadError() const override;
  PrefReadError ReadPrefs() override;
  void ReadPrefsAsync(ReadErrorDelegate* error_delegate) override;
  void CommitPendingWrite() override;
  void SchedulePendingLossyWrites() override;
  void ClearMutableValues() override;

 protected:
  // base::RefCounted<PrefStore>:
  ~PersistentPrefStoreClient() override;

 private:
  void OnConnect(mojom::PersistentPrefStoreConnectionPtr connection,
                 std::unordered_map<PrefValueStore::PrefStoreType,
                                    prefs::mojom::PrefStoreConnectionPtr>
                     other_pref_stores);

  mojom::PrefStoreConnectorPtr connector_;
  bool read_only_ = false;
  PrefReadError read_error_ = PersistentPrefStore::PREF_READ_ERROR_NONE;
  mojom::PersistentPrefStorePtr pref_store_;

  std::unique_ptr<ReadErrorDelegate> error_delegate_;

  DISALLOW_COPY_AND_ASSIGN(PersistentPrefStoreClient);
};

}  // namespace prefs

#endif  // SERVICES_PREFERENCES_PUBLIC_CPP_PERSISTENT_PREF_STORE_CLIENT_H_
