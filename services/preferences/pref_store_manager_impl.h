// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_PREF_STORE_MANAGER_IMPL_H_
#define SERVICES_PREFERENCES_PREF_STORE_MANAGER_IMPL_H_

#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/prefs/pref_value_store.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/strong_binding_set.h"
#include "services/preferences/public/interfaces/preferences.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"

namespace base {
class SequencedWorkerPool;
}

namespace prefs {
class SharedPrefRegistry;
class PersistentPrefStoreImpl;
class ScopedPrefConnectionBuilder;

// This class mediates the connection of clients who wants to read preferences
// and the pref stores that store those preferences. Pref stores use the
// |PrefStoreRegistry| interface to register themselves with the manager and
// clients use the |PrefStoreConnector| interface to connect to these stores.
class PrefStoreManagerImpl : public mojom::PrefStoreRegistry,
                             public mojom::PrefServiceControl,
                             public service_manager::Service {
 public:
  // Only replies to Connect calls when all |expected_pref_stores| have
  // registered. |expected_pref_stores| must contain
  // PrefValueStore::DEFAULT_STORE and PrefValueStore::USER_STORE for
  // consistency, as the service always registers these
  // internally. |worker_pool| is used for any I/O performed by the service.
  PrefStoreManagerImpl(
      std::set<PrefValueStore::PrefStoreType> expected_pref_stores,
      scoped_refptr<base::SequencedWorkerPool> worker_pool);
  ~PrefStoreManagerImpl() override;

 private:
  class ConnectorConnection;

  // mojom::PrefStoreRegistry:
  void Register(PrefValueStore::PrefStoreType type,
                mojom::PrefStorePtr pref_store_ptr) override;

  void BindPrefStoreConnectorRequest(
      const service_manager::BindSourceInfo& source_info,
      prefs::mojom::PrefStoreConnectorRequest request);
  void BindPrefStoreRegistryRequest(
      const service_manager::BindSourceInfo& source_info,
      prefs::mojom::PrefStoreRegistryRequest request);
  void BindPrefServiceControlRequest(
      const service_manager::BindSourceInfo& source_info,
      prefs::mojom::PrefServiceControlRequest request);

  // PrefServiceControl:
  void Init(mojom::PersistentPrefStoreConfigurationPtr configuration) override;

  // service_manager::Service:
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  // Called when a PrefStore previously registered using |Register| disconnects.
  void OnPrefStoreDisconnect(PrefValueStore::PrefStoreType type);

  void OnPersistentPrefStoreReady();

  // Has |Init| been called?
  bool Initialized() const;

  // PrefStores that need to register before replying to any Connect calls. This
  // does not include the PersistentPrefStore, which is handled separately.
  std::set<PrefValueStore::PrefStoreType> expected_pref_stores_;

  // Registered pref stores.
  std::unordered_map<PrefValueStore::PrefStoreType, mojom::PrefStorePtr>
      pref_store_ptrs_;

  mojo::StrongBindingSet<mojom::PrefStoreConnector> connector_bindings_;
  mojo::BindingSet<mojom::PrefStoreRegistry> registry_bindings_;
  std::unique_ptr<PersistentPrefStoreImpl> persistent_pref_store_;
  mojo::Binding<mojom::PrefServiceControl> init_binding_;

  mojom::PrefStoreConnectorPtr incognito_connector_;

  const std::unique_ptr<SharedPrefRegistry> shared_pref_registry_;

  // The same |ScopedPrefConnectionBuilder| instance may appear multiple times
  // in |pending_connections_|, once per type of pref store it's waiting for,
  // and at most once in |pending_persistent_connections_|.
  std::unordered_map<PrefValueStore::PrefStoreType,
                     std::vector<scoped_refptr<ScopedPrefConnectionBuilder>>>
      pending_connections_;
  std::vector<scoped_refptr<ScopedPrefConnectionBuilder>>
      pending_persistent_connections_;
  std::vector<scoped_refptr<ScopedPrefConnectionBuilder>>
      pending_incognito_connections_;

  const scoped_refptr<base::SequencedWorkerPool> worker_pool_;

  service_manager::BinderRegistry registry_;

  DISALLOW_COPY_AND_ASSIGN(PrefStoreManagerImpl);
};

}  // namespace prefs

#endif  // SERVICES_PREFERENCES_PREF_STORE_MANAGER_IMPL_H_
