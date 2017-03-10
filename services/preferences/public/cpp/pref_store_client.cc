// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/public/cpp/pref_store_client.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/preferences/public/cpp/pref_store_impl.h"

namespace prefs {

PrefStoreClient::PrefStoreClient(mojom::PrefStoreConnectionPtr connection)
    : cached_prefs_(std::move(connection->initial_prefs)),
      initialized_(connection->is_initialized),
      observer_binding_(this, std::move(connection->observer)) {}

void PrefStoreClient::AddObserver(PrefStore::Observer* observer) {
  observers_.AddObserver(observer);
}

void PrefStoreClient::RemoveObserver(PrefStore::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool PrefStoreClient::HasObservers() const {
  return observers_.might_have_observers();
}

bool PrefStoreClient::IsInitializationComplete() const {
  return initialized_;
}

bool PrefStoreClient::GetValue(const std::string& key,
                               const base::Value** result) const {
  DCHECK(initialized_);
  return cached_prefs_->Get(key, result);
}

std::unique_ptr<base::DictionaryValue> PrefStoreClient::GetValues() const {
  DCHECK(initialized_);
  return cached_prefs_->CreateDeepCopy();
}

PrefStoreClient::~PrefStoreClient() = default;

void PrefStoreClient::OnPrefChanged(const std::string& key,
                                    std::unique_ptr<base::Value> value) {
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
  if (changed) {
    for (Observer& observer : observers_)
      observer.OnPrefValueChanged(key);
  }
}

void PrefStoreClient::OnInitializationCompleted(bool succeeded) {
  if (!initialized_) {
    initialized_ = true;
    for (Observer& observer : observers_)
      observer.OnInitializationCompleted(true);
  }
}

}  // namespace prefs
