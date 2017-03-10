// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_PUBLIC_CPP_PREF_STORE_ADAPTER_H_
#define SERVICES_PREFERENCES_PUBLIC_CPP_PREF_STORE_ADAPTER_H_

#include <memory>

#include "base/macros.h"
#include "base/values.h"
#include "components/prefs/pref_store.h"
#include "services/preferences/public/cpp/pref_store_impl.h"

namespace prefs {

// Ties the lifetime of a |PrefStoreImpl| to a |PrefStore|. Otherwise acts as a
// |PrefStore|, forwarding all calls to the wrapped |PrefStore|.
class PrefStoreAdapter : public PrefStore {
 public:
  PrefStoreAdapter(scoped_refptr<PrefStore> pref_store,
                   std::unique_ptr<PrefStoreImpl> impl);

  // PrefStore:
  void AddObserver(PrefStore::Observer* observer) override;
  void RemoveObserver(PrefStore::Observer* observer) override;
  bool HasObservers() const override;
  bool IsInitializationComplete() const override;
  bool GetValue(const std::string& key,
                const base::Value** result) const override;
  std::unique_ptr<base::DictionaryValue> GetValues() const override;

 private:
  ~PrefStoreAdapter() override;

  scoped_refptr<PrefStore> pref_store_;
  std::unique_ptr<PrefStoreImpl> impl_;

  DISALLOW_COPY_AND_ASSIGN(PrefStoreAdapter);
};

}  // namespace prefs

#endif  // SERVICES_PREFERENCES_PUBLIC_CPP_PREF_STORE_ADAPTER_H_
