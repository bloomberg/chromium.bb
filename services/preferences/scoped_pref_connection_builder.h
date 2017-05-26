// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_SCOPED_PREF_CONNECTION_BUILDER_H_
#define SERVICES_PREFERENCES_SCOPED_PREF_CONNECTION_BUILDER_H_

#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "services/preferences/public/interfaces/preferences.mojom.h"

namespace prefs {
class PersistentPrefStoreImpl;

// A builder for connections to pref stores. When all references are released,
// the connection is created.
class ScopedPrefConnectionBuilder
    : public base::RefCounted<ScopedPrefConnectionBuilder> {
 public:
  // |required_types| lists all the |Provide*| calls the client of this object
  // is expected to make. Once all these |Provide*| calls have been made, the
  // |callback| will be invoked.
  ScopedPrefConnectionBuilder(
      std::vector<std::string> observed_prefs,
      std::set<PrefValueStore::PrefStoreType> required_types,
      mojom::PrefStoreConnector::ConnectCallback callback);

  std::set<PrefValueStore::PrefStoreType> ProvidePrefStoreConnections(
      const std::unordered_map<PrefValueStore::PrefStoreType,
                               mojom::PrefStorePtr>& pref_stores);

  void ProvidePrefStoreConnection(PrefValueStore::PrefStoreType type,
                                  mojom::PrefStore* ptr);

  void ProvidePersistentPrefStore(
      PersistentPrefStoreImpl* persistent_pref_store);

 private:
  friend class base::RefCounted<ScopedPrefConnectionBuilder>;
  ~ScopedPrefConnectionBuilder();

  void OnConnect(PrefValueStore::PrefStoreType type,
                 mojom::PrefStoreConnectionPtr connection_ptr);

  mojom::PrefStoreConnector::ConnectCallback callback_;
  std::vector<std::string> observed_prefs_;

  std::set<PrefValueStore::PrefStoreType> required_types_;

  std::unordered_map<PrefValueStore::PrefStoreType,
                     mojom::PrefStoreConnectionPtr>
      connections_;

  mojom::PersistentPrefStoreConnectionPtr persistent_pref_store_connection_;

  DISALLOW_COPY_AND_ASSIGN(ScopedPrefConnectionBuilder);
};

}  // namespace prefs

#endif  // SERVICES_PREFERENCES_SCOPED_PREF_CONNECTION_BUILDER_H_
