// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/public/cpp/pref_client_store.h"

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "services/service_manager/public/cpp/connector.h"

namespace preferences {

PrefClientStore::PrefClientStore(
    prefs::mojom::PreferencesServiceFactoryPtr pref_factory_ptr)
    : prefs_binding_(this),
      pref_factory_ptr_(std::move(pref_factory_ptr)),
      initialized_(false) {
  pref_factory_ptr_->Create(prefs_binding_.CreateInterfacePtrAndBind(),
                            mojo::MakeRequest(&prefs_service_ptr_));
}

void PrefClientStore::Subscribe(const std::set<std::string>& keys) {
  keys_.insert(keys.begin(), keys.end());

  std::vector<std::string> pref_array;
  std::copy(keys_.begin(), keys_.end(), std::back_inserter(pref_array));
  prefs_service_ptr_->Subscribe(pref_array);
}

bool PrefClientStore::GetValue(const std::string& key,
                               const base::Value** value) const {
  DCHECK(initialized_);
  DCHECK(keys_.find(key) != keys_.end());

  return ValueMapPrefStore::GetValue(key, value);
}

void PrefClientStore::SetValue(const std::string& key,
                               std::unique_ptr<base::Value> value,
                               uint32_t flags) {
  DCHECK(keys_.find(key) != keys_.end());

  // TODO(jonross): only notify the server if the value changed.
  SetValueOnPreferenceManager(key, *value);
  ValueMapPrefStore::SetValue(key, std::move(value), flags);
}

void PrefClientStore::RemoveValue(const std::string& key, uint32_t flags) {
  // TODO(jonross): add preference removal to preferences.mojom
  NOTIMPLEMENTED();
}

bool PrefClientStore::GetMutableValue(const std::string& key,
                                      base::Value** value) {
  DCHECK(initialized_);
  DCHECK(keys_.find(key) != keys_.end());

  return ValueMapPrefStore::GetMutableValue(key, value);
}

void PrefClientStore::ReportValueChanged(const std::string& key,
                                         uint32_t flags) {
  DCHECK(keys_.find(key) != keys_.end());
  const base::Value* value = nullptr;
  ValueMapPrefStore::GetValue(key, &value);
  SetValueOnPreferenceManager(key, *value);
  ValueMapPrefStore::ReportValueChanged(key, flags);
}

void PrefClientStore::SetValueSilently(const std::string& key,
                                       std::unique_ptr<base::Value> value,
                                       uint32_t flags) {
  SetValueOnPreferenceManager(key, *value);
  ValueMapPrefStore::SetValueSilently(key, std::move(value), flags);
}

PrefClientStore::~PrefClientStore() {}

void PrefClientStore::SetValueOnPreferenceManager(const std::string& key,
                                                  const base::Value& value) {
  if (keys_.find(key) == keys_.end())
    return;

  auto prefs = base::MakeUnique<base::DictionaryValue>();
  prefs->SetWithoutPathExpansion(key, value.CreateDeepCopy());
  prefs_service_ptr_->SetPreferences(std::move(prefs));
}

void PrefClientStore::OnPreferencesChanged(
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
