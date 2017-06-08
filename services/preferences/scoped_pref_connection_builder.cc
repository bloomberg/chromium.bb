// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/scoped_pref_connection_builder.h"

#include "services/preferences/persistent_pref_store_impl.h"

namespace prefs {

ScopedPrefConnectionBuilder::ScopedPrefConnectionBuilder(
    std::vector<std::string> observed_prefs,
    std::set<PrefValueStore::PrefStoreType> required_types,
    mojom::PrefStoreConnector::ConnectCallback callback)
    : callback_(std::move(callback)),
      observed_prefs_(std::move(observed_prefs)),
      required_types_(std::move(required_types)) {}

std::set<PrefValueStore::PrefStoreType>
ScopedPrefConnectionBuilder::ProvidePrefStoreConnections(
    const std::unordered_map<PrefValueStore::PrefStoreType,
                             mojom::PrefStorePtr>& pref_stores) {
  for (const auto& pref_store : pref_stores) {
    auto it = required_types_.find(pref_store.first);
    if (it != required_types_.end()) {
      ProvidePrefStoreConnection(pref_store.first, pref_store.second.get());
      required_types_.erase(it);
    }
  }
  return std::move(required_types_);
}

void ScopedPrefConnectionBuilder::ProvidePrefStoreConnection(
    PrefValueStore::PrefStoreType type,
    mojom::PrefStore* pref_store) {
  pref_store->AddObserver(
      observed_prefs_,
      base::Bind(&ScopedPrefConnectionBuilder::OnConnect, this, type));
}

void ScopedPrefConnectionBuilder::ProvidePersistentPrefStore(
    PersistentPrefStoreImpl* persistent_pref_store) {
  DCHECK(!persistent_pref_store_connection_);
  persistent_pref_store_connection_ = persistent_pref_store->CreateConnection(
      PersistentPrefStoreImpl::ObservedPrefs(observed_prefs_.begin(),
                                             observed_prefs_.end()));
}

void ScopedPrefConnectionBuilder::ProvideIncognitoConnector(
    const mojom::PrefStoreConnectorPtr& incognito_connector) {
  incognito_connector->ConnectToUserPrefStore(
      observed_prefs_,
      base::Bind(&ScopedPrefConnectionBuilder::OnIncognitoConnect, this));
}

void ScopedPrefConnectionBuilder::ProvideDefaults(
    std::vector<mojom::PrefRegistrationPtr> defaults) {
  defaults_ = std::move(defaults);
}

ScopedPrefConnectionBuilder::~ScopedPrefConnectionBuilder() {
  std::move(callback_).Run(std::move(persistent_pref_store_connection_),
                           std::move(incognito_connection_),
                           std::move(defaults_), std::move(connections_));
}

void ScopedPrefConnectionBuilder::OnConnect(
    PrefValueStore::PrefStoreType type,
    mojom::PrefStoreConnectionPtr connection_ptr) {
  bool inserted = connections_.emplace(type, std::move(connection_ptr)).second;
  DCHECK(inserted);
}

void ScopedPrefConnectionBuilder::OnIncognitoConnect(
    mojom::PersistentPrefStoreConnectionPtr connection_ptr) {
  DCHECK(!incognito_connection_);
  incognito_connection_ = std::move(connection_ptr);
}

}  // namespace prefs
