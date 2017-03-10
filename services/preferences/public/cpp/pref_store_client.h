// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_PUBLIC_CPP_PREF_STORE_CLIENT_H_
#define SERVICES_PREFERENCES_PUBLIC_CPP_PREF_STORE_CLIENT_H_

#include <string>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "components/prefs/pref_store.h"
#include "components/prefs/pref_value_map.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/preferences/public/cpp/pref_store_manager_impl.h"
#include "services/preferences/public/interfaces/preferences.mojom.h"

namespace prefs {

// TODO(tibell): Make PrefObserverStore use PrefStoreClient as a base class.

// An implementation of PrefStore which uses prefs::mojom::PrefStore as
// the backing store of the preferences.
//
// PrefStoreClient provides synchronous access to the preferences stored by the
// backing store by caching them locally.
class PrefStoreClient : public ::PrefStore, public mojom::PrefStoreObserver {
 public:
  explicit PrefStoreClient(mojom::PrefStoreConnectionPtr connection);

  // PrefStore:
  void AddObserver(PrefStore::Observer* observer) override;
  void RemoveObserver(PrefStore::Observer* observer) override;
  bool HasObservers() const override;
  bool IsInitializationComplete() const override;
  bool GetValue(const std::string& key,
                const base::Value** result) const override;
  std::unique_ptr<base::DictionaryValue> GetValues() const override;

 private:
  friend class PrefStoreClientTest;

  ~PrefStoreClient() override;

  // prefs::mojom::PreferenceObserver:
  void OnPrefChanged(const std::string& key,
                     std::unique_ptr<base::Value> value) override;
  void OnInitializationCompleted(bool succeeded) override;

  // Cached preferences.
  std::unique_ptr<base::DictionaryValue> cached_prefs_;

  base::ObserverList<PrefStore::Observer, true> observers_;

  // Has the PrefStore we're observing been initialized?
  bool initialized_;

  mojo::Binding<mojom::PrefStoreObserver> observer_binding_;

  DISALLOW_COPY_AND_ASSIGN(PrefStoreClient);
};

}  // namespace prefs

#endif  // SERVICES_PREFERENCES_PUBLIC_CPP_PREF_STORE_CLIENT_H_
