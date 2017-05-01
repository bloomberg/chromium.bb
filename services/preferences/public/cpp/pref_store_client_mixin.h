// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_PUBLIC_CPP_PREF_STORE_CLIENT_MIXIN_H_
#define SERVICES_PREFERENCES_PUBLIC_CPP_PREF_STORE_CLIENT_MIXIN_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/preferences/public/interfaces/preferences.mojom.h"

namespace base {
class DictionaryValue;
class Value;
}

namespace prefs {

// An mixin implementation of PrefStore which uses prefs::mojom::PrefStore as
// the backing store of the preferences.
//
// PrefStoreClientMixin provides synchronous access to the preferences stored by
// the backing store by caching them locally.
template <typename BasePrefStore>
class PrefStoreClientMixin : public BasePrefStore,
                             public mojom::PrefStoreObserver {
 public:
  PrefStoreClientMixin();

  // BasePrefStore:
  void AddObserver(PrefStore::Observer* observer) override;
  void RemoveObserver(PrefStore::Observer* observer) override;
  bool HasObservers() const override;
  bool IsInitializationComplete() const override;
  bool GetValue(const std::string& key,
                const base::Value** result) const override;
  std::unique_ptr<base::DictionaryValue> GetValues() const override;

 protected:
  ~PrefStoreClientMixin() override;

  // Initializes |this|. This must be called before any of the other public or
  // protected member functions.
  void Init(std::unique_ptr<base::DictionaryValue> initial_prefs,
            bool initialized,
            mojom::PrefStoreObserverRequest observer_request);

  base::DictionaryValue& GetMutableValues();
  void ReportPrefValueChanged(const std::string& key);

 private:
  // prefs::mojom::PreferenceObserver:
  void OnPrefsChanged(std::vector<mojom::PrefUpdatePtr> updates) override;
  void OnInitializationCompleted(bool succeeded) override;

  void OnPrefChanged(const std::string& key,
                     mojom::PrefUpdateValuePtr update_value);

  // Cached preferences.
  // If null, indicates that initialization failed.
  std::unique_ptr<base::DictionaryValue> cached_prefs_;

  base::ObserverList<PrefStore::Observer, true> observers_;

  // Has the PrefStore we're observing been initialized?
  bool initialized_ = false;

  mojo::Binding<mojom::PrefStoreObserver> observer_binding_;

  DISALLOW_COPY_AND_ASSIGN(PrefStoreClientMixin);
};

extern template class PrefStoreClientMixin<::PrefStore>;
extern template class PrefStoreClientMixin<::PersistentPrefStore>;

}  // namespace prefs

#endif  // SERVICES_PREFERENCES_PUBLIC_CPP_PREF_STORE_CLIENT_MIXIN_H_
