// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/public/cpp/pref_observer_store.h"

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "services/service_manager/public/cpp/connector.h"

namespace preferences {

PrefObserverStore::PrefObserverStore(
    prefs::mojom::PreferencesManagerPtr prefs_manager_ptr)
    : prefs_binding_(this),
      prefs_manager_ptr_(std::move(prefs_manager_ptr)),
      initialized_(false) {}

void PrefObserverStore::Subscribe(const std::set<std::string>& keys) {
  if (keys_.empty())
    prefs_manager_ptr_->AddObserver(prefs_binding_.CreateInterfacePtrAndBind());
  keys_.insert(keys.begin(), keys.end());

  std::vector<std::string> pref_array;
  std::copy(keys_.begin(), keys_.end(), std::back_inserter(pref_array));
  prefs_manager_ptr_->Subscribe(pref_array);
}

bool PrefObserverStore::GetValue(const std::string& key,
                                 const base::Value** value) const {
  DCHECK(initialized_);
  DCHECK(keys_.find(key) != keys_.end());

  return ValueMapPrefStore::GetValue(key, value);
}

void PrefObserverStore::SetValue(const std::string& key,
                                 std::unique_ptr<base::Value> value,
                                 uint32_t flags) {
  DCHECK(keys_.find(key) != keys_.end());

  // TODO(jonross): only notify the server if the value changed.
  SetValueOnPreferenceManager(key, *value);
  ValueMapPrefStore::SetValue(key, std::move(value), flags);
}

void PrefObserverStore::RemoveValue(const std::string& key, uint32_t flags) {
  // TODO(jonross): add preference removal to preferences.mojom
  NOTIMPLEMENTED();
}

bool PrefObserverStore::GetMutableValue(const std::string& key,
                                        base::Value** value) {
  DCHECK(initialized_);
  DCHECK(keys_.find(key) != keys_.end());

  return ValueMapPrefStore::GetMutableValue(key, value);
}

void PrefObserverStore::ReportValueChanged(const std::string& key,
                                           uint32_t flags) {
  DCHECK(keys_.find(key) != keys_.end());
  const base::Value* value = nullptr;
  ValueMapPrefStore::GetValue(key, &value);
  SetValueOnPreferenceManager(key, *value);
  ValueMapPrefStore::ReportValueChanged(key, flags);
}

void PrefObserverStore::SetValueSilently(const std::string& key,
                                         std::unique_ptr<base::Value> value,
                                         uint32_t flags) {
  SetValueOnPreferenceManager(key, *value);
  ValueMapPrefStore::SetValueSilently(key, std::move(value), flags);
}

PrefObserverStore::~PrefObserverStore() {}

void PrefObserverStore::SetValueOnPreferenceManager(const std::string& key,
                                                    const base::Value& value) {
  if (keys_.find(key) == keys_.end())
    return;

  auto prefs = base::MakeUnique<base::DictionaryValue>();
  prefs->Set(key, value.CreateDeepCopy());
  prefs_manager_ptr_->SetPreferences(std::move(prefs));
}

void PrefObserverStore::OnPreferencesChanged(
    std::unique_ptr<base::DictionaryValue> preferences) {
  if (!initialized_) {
    initialized_ = true;
    NotifyInitializationCompleted();
  }

  for (base::DictionaryValue::Iterator it(*preferences); !it.IsAtEnd();
       it.Advance()) {
    if (keys_.find(it.key()) == keys_.end())
      continue;
    ValueMapPrefStore::SetValue(it.key(), it.value().CreateDeepCopy(), 0);
  }
}

}  // namespace preferences
