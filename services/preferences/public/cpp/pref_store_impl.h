// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_PUBLIC_CPP_PREF_STORE_IMPL_H_
#define SERVICES_PREFERENCES_PUBLIC_CPP_PREF_STORE_IMPL_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/prefs/pref_store.h"
#include "components/prefs/pref_value_store.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/preferences/public/interfaces/preferences.mojom.h"

namespace prefs {

// Wraps an actual PrefStore implementation and exposes it as a
// mojom::PrefStore interface.
class PrefStoreImpl : public ::PrefStore::Observer, public mojom::PrefStore {
 public:
  PrefStoreImpl(scoped_refptr<::PrefStore> pref_store,
                mojom::PrefStoreRequest request);
  ~PrefStoreImpl() override;

  // The created instance is registered in and owned by the
  // |mojom::PrefStoreRegistry|.
  static std::unique_ptr<PrefStoreImpl> Create(
      mojom::PrefStoreRegistry* registry_ptr,
      scoped_refptr<::PrefStore> pref_store,
      PrefValueStore::PrefStoreType type);

 private:
  class Observer;

  // PrefStore::Observer:
  void OnPrefValueChanged(const std::string& key) override;
  void OnInitializationCompleted(bool succeeded) override;

  // prefs::mojom::PrefStore:
  void AddObserver(const std::vector<std::string>& prefs_to_observe,
                   const AddObserverCallback& callback) override;

  // The backing store we observer for changes.
  scoped_refptr<::PrefStore> backing_pref_store_;

  // Observers we notify when |backing_pref_store_| changes.
  std::vector<std::unique_ptr<Observer>> observers_;

  // True when the |backing_pref_store_| is initialized, either because it was
  // passed already initialized in the constructor or after
  // OnInitializationCompleted was called.
  bool backing_pref_store_initialized_;

  mojo::Binding<mojom::PrefStore> binding_;

  DISALLOW_COPY_AND_ASSIGN(PrefStoreImpl);
};

}  // namespace prefs

#endif  // SERVICES_PREFERENCES_PUBLIC_CPP_PREF_STORE_IMPL_H_
