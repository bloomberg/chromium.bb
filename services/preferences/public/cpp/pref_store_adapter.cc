// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/public/cpp/pref_store_adapter.h"

namespace prefs {

PrefStoreAdapter::PrefStoreAdapter(scoped_refptr<PrefStore> pref_store,
                                   std::unique_ptr<PrefStoreImpl> impl)
    : pref_store_(std::move(pref_store)), impl_(std::move(impl)) {}

void PrefStoreAdapter::AddObserver(PrefStore::Observer* observer) {
  pref_store_->AddObserver(observer);
}

void PrefStoreAdapter::RemoveObserver(PrefStore::Observer* observer) {
  pref_store_->RemoveObserver(observer);
}

bool PrefStoreAdapter::HasObservers() const {
  return pref_store_->HasObservers();
}

bool PrefStoreAdapter::IsInitializationComplete() const {
  return pref_store_->IsInitializationComplete();
}

bool PrefStoreAdapter::GetValue(const std::string& key,
                                const base::Value** result) const {
  return pref_store_->GetValue(key, result);
}

std::unique_ptr<base::DictionaryValue> PrefStoreAdapter::GetValues() const {
  return pref_store_->GetValues();
}

PrefStoreAdapter::~PrefStoreAdapter() = default;

}  // namespace prefs
