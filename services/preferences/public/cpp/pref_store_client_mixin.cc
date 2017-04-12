// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/public/cpp/pref_store_client_mixin.h"

#include <utility>

#include "base/values.h"
#include "services/preferences/public/cpp/pref_store_client.h"

namespace prefs {

template <typename BasePrefStore>
PrefStoreClientMixin<BasePrefStore>::PrefStoreClientMixin()
    : observer_binding_(this) {}

template <typename BasePrefStore>
void PrefStoreClientMixin<BasePrefStore>::AddObserver(
    PrefStore::Observer* observer) {
  observers_.AddObserver(observer);
}

template <typename BasePrefStore>
void PrefStoreClientMixin<BasePrefStore>::RemoveObserver(
    PrefStore::Observer* observer) {
  observers_.RemoveObserver(observer);
}

template <typename BasePrefStore>
bool PrefStoreClientMixin<BasePrefStore>::HasObservers() const {
  return observers_.might_have_observers();
}

template <typename BasePrefStore>
bool PrefStoreClientMixin<BasePrefStore>::IsInitializationComplete() const {
  return initialized_ && static_cast<bool>(cached_prefs_);
}

template <typename BasePrefStore>
bool PrefStoreClientMixin<BasePrefStore>::GetValue(
    const std::string& key,
    const base::Value** result) const {
  DCHECK(initialized_);
  DCHECK(cached_prefs_);
  return cached_prefs_->Get(key, result);
}

template <typename BasePrefStore>
std::unique_ptr<base::DictionaryValue>
PrefStoreClientMixin<BasePrefStore>::GetValues() const {
  DCHECK(initialized_);
  DCHECK(cached_prefs_);
  return cached_prefs_->CreateDeepCopy();
}

template <typename BasePrefStore>
PrefStoreClientMixin<BasePrefStore>::~PrefStoreClientMixin() = default;

template <typename BasePrefStore>
void PrefStoreClientMixin<BasePrefStore>::Init(
    std::unique_ptr<base::DictionaryValue> initial_prefs,
    bool initialized,
    mojom::PrefStoreObserverRequest observer_request) {
  cached_prefs_ = std::move(initial_prefs);
  observer_binding_.Bind(std::move(observer_request));
  if (initialized)
    OnInitializationCompleted(static_cast<bool>(cached_prefs_));
}

template <typename BasePrefStore>
base::DictionaryValue& PrefStoreClientMixin<BasePrefStore>::GetMutableValues() {
  DCHECK(cached_prefs_);
  return *cached_prefs_;
}

template <typename BasePrefStore>
void PrefStoreClientMixin<BasePrefStore>::ReportPrefValueChanged(
    const std::string& key) {
  for (auto& observer : observers_)
    observer.OnPrefValueChanged(key);
}

template <typename BasePrefStore>
void PrefStoreClientMixin<BasePrefStore>::OnPrefsChanged(
    std::vector<mojom::PrefUpdatePtr> updates) {
  for (const auto& update : updates)
    OnPrefChanged(update->key, std::move(update->value));
}

template <typename BasePrefStore>
void PrefStoreClientMixin<BasePrefStore>::OnInitializationCompleted(
    bool succeeded) {
  if (!initialized_) {
    initialized_ = true;
    for (auto& observer : observers_)
      observer.OnInitializationCompleted(succeeded);
  }
}

template <typename BasePrefStore>
void PrefStoreClientMixin<BasePrefStore>::OnPrefChanged(
    const std::string& key,
    std::unique_ptr<base::Value> value) {
  DCHECK(cached_prefs_);
  bool changed = false;
  if (!value) {  // Delete
    if (cached_prefs_->RemovePath(key, nullptr))
      changed = true;
  } else {
    const base::Value* prev;
    if (cached_prefs_->Get(key, &prev)) {
      if (!prev->Equals(value.get())) {
        cached_prefs_->Set(key, std::move(value));
        changed = true;
      }
    } else {
      cached_prefs_->Set(key, std::move(value));
      changed = true;
    }
  }
  if (changed && initialized_)
    ReportPrefValueChanged(key);
}

template class PrefStoreClientMixin<::PrefStore>;
template class PrefStoreClientMixin<::PersistentPrefStore>;

}  // namespace prefs
