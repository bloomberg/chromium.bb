// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/pref_store_manager_impl.h"

#include <algorithm>
#include <utility>

#include "base/memory/ref_counted.h"
#include "base/stl_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "components/prefs/pref_value_store.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/preferences/persistent_pref_store_factory.h"
#include "services/preferences/persistent_pref_store_impl.h"
#include "services/preferences/scoped_pref_connection_builder.h"
#include "services/preferences/shared_pref_registry.h"
#include "services/service_manager/public/cpp/bind_source_info.h"

namespace prefs {

class PrefStoreManagerImpl::ConnectorConnection
    : public mojom::PrefStoreConnector {
 public:
  ConnectorConnection(PrefStoreManagerImpl* owner,
                      const service_manager::BindSourceInfo& source_info)
      : owner_(owner), source_info_(source_info) {}

  // mojom::PrefStoreConnector: |already_connected_types| must not include
  // PrefValueStore::DEFAULT_STORE and PrefValueStore::USER_STORE as these must
  // always be accessed through the service.
  void Connect(
      mojom::PrefRegistryPtr pref_registry,
      const std::vector<PrefValueStore::PrefStoreType>& already_connected_types,
      ConnectCallback callback) override {
    std::set<PrefValueStore::PrefStoreType> required_remote_types;
    for (auto type : owner_->expected_pref_stores_) {
      if (!base::ContainsValue(already_connected_types, type)) {
        required_remote_types.insert(type);
      }
    }
    auto connection = owner_->shared_pref_registry_->CreateConnectionBuilder(
        std::move(pref_registry), std::move(required_remote_types),
        source_info_.identity, std::move(callback));
    if (owner_->persistent_pref_store_ &&
        owner_->persistent_pref_store_->initialized()) {
      connection->ProvidePersistentPrefStore(
          owner_->persistent_pref_store_.get());
    } else {
      owner_->pending_persistent_connections_.push_back(connection);
    }
    auto remaining_remote_types =
        connection->ProvidePrefStoreConnections(owner_->pref_store_ptrs_);
    for (auto type : remaining_remote_types) {
      owner_->pending_connections_[type].push_back(connection);
    }
    if (!owner_->Initialized()) {
      owner_->pending_incognito_connections_.push_back(connection);
    } else if (owner_->incognito_connector_) {
      connection->ProvideIncognitoConnector(owner_->incognito_connector_);
    }
  }

  void ConnectToUserPrefStore(
      const std::vector<std::string>& observed_prefs,
      mojom::PrefStoreConnector::ConnectToUserPrefStoreCallback callback)
      override {
    std::move(callback).Run(owner_->persistent_pref_store_->CreateConnection(
        PersistentPrefStoreImpl::ObservedPrefs(observed_prefs.begin(),
                                               observed_prefs.end())));
  }

 private:
  PrefStoreManagerImpl* const owner_;
  const service_manager::BindSourceInfo source_info_;
};

PrefStoreManagerImpl::PrefStoreManagerImpl(
    std::set<PrefValueStore::PrefStoreType> expected_pref_stores,
    scoped_refptr<base::SequencedWorkerPool> worker_pool)
    : expected_pref_stores_(std::move(expected_pref_stores)),
      init_binding_(this),
      shared_pref_registry_(base::MakeUnique<SharedPrefRegistry>()),
      worker_pool_(std::move(worker_pool)) {
  DCHECK(
      base::ContainsValue(expected_pref_stores_, PrefValueStore::USER_STORE) &&
      base::ContainsValue(expected_pref_stores_, PrefValueStore::DEFAULT_STORE))
      << "expected_pref_stores must always include PrefValueStore::USER_STORE "
         "and PrefValueStore::DEFAULT_STORE.";
  // The user store is not actually registered or connected to in the
  // implementation, but accessed directly.
  expected_pref_stores_.erase(PrefValueStore::USER_STORE);
  DVLOG(1) << "Expecting " << expected_pref_stores_.size()
           << " pref store(s) to register";
  // This store is done in-process so it's already "registered":
  DVLOG(1) << "Registering pref store: " << PrefValueStore::DEFAULT_STORE;
  registry_.AddInterface<prefs::mojom::PrefStoreConnector>(
      base::Bind(&PrefStoreManagerImpl::BindPrefStoreConnectorRequest,
                 base::Unretained(this)));
  registry_.AddInterface<prefs::mojom::PrefStoreRegistry>(
      base::Bind(&PrefStoreManagerImpl::BindPrefStoreRegistryRequest,
                 base::Unretained(this)));
  registry_.AddInterface<prefs::mojom::PrefServiceControl>(
      base::Bind(&PrefStoreManagerImpl::BindPrefServiceControlRequest,
                 base::Unretained(this)));
}

PrefStoreManagerImpl::~PrefStoreManagerImpl() {
  // For logging consistency:
  DVLOG(1) << "Deregistering pref store: " << PrefValueStore::DEFAULT_STORE;
}

void PrefStoreManagerImpl::Register(PrefValueStore::PrefStoreType type,
                                    mojom::PrefStorePtr pref_store_ptr) {
  if (expected_pref_stores_.count(type) == 0) {
    LOG(WARNING) << "Not registering unexpected pref store: " << type;
    return;
  }
  DVLOG(1) << "Registering pref store: " << type;
  pref_store_ptr.set_connection_error_handler(
      base::Bind(&PrefStoreManagerImpl::OnPrefStoreDisconnect,
                 base::Unretained(this), type));
  auto it = pending_connections_.find(type);
  if (it != pending_connections_.end()) {
    for (const auto& connection : it->second)
      connection->ProvidePrefStoreConnection(type, pref_store_ptr.get());
    pending_connections_.erase(it);
  }

  const bool success =
      pref_store_ptrs_.insert(std::make_pair(type, std::move(pref_store_ptr)))
          .second;
  DCHECK(success) << "The same pref store registered twice: " << type;
}

void PrefStoreManagerImpl::BindPrefStoreConnectorRequest(
    const service_manager::BindSourceInfo& source_info,
    prefs::mojom::PrefStoreConnectorRequest request) {
  connector_bindings_.AddBinding(
      base::MakeUnique<ConnectorConnection>(this, source_info),
      std::move(request));
}

void PrefStoreManagerImpl::BindPrefStoreRegistryRequest(
    const service_manager::BindSourceInfo& source_info,
    prefs::mojom::PrefStoreRegistryRequest request) {
  registry_bindings_.AddBinding(this, std::move(request));
}

void PrefStoreManagerImpl::BindPrefServiceControlRequest(
    const service_manager::BindSourceInfo& source_info,
    prefs::mojom::PrefServiceControlRequest request) {
  if (init_binding_.is_bound()) {
    LOG(ERROR)
        << "Pref service received unexpected control interface connection from "
        << source_info.identity.name();
    return;
  }

  init_binding_.Bind(std::move(request));
}

void PrefStoreManagerImpl::Init(
    mojom::PersistentPrefStoreConfigurationPtr configuration) {
  DCHECK(!persistent_pref_store_);

  if (configuration->is_incognito_configuration()) {
    incognito_connector_ =
        std::move(configuration->get_incognito_configuration()->connector);
    for (auto connection : pending_incognito_connections_) {
      connection->ProvideIncognitoConnector(incognito_connector_);
    }
  }
  pending_incognito_connections_.clear();
  persistent_pref_store_ = CreatePersistentPrefStore(
      std::move(configuration), worker_pool_.get(),
      base::Bind(&PrefStoreManagerImpl::OnPersistentPrefStoreReady,
                 base::Unretained(this)));
  DCHECK(persistent_pref_store_);
  if (persistent_pref_store_->initialized()) {
    OnPersistentPrefStoreReady();
  }
}

void PrefStoreManagerImpl::OnStart() {}

void PrefStoreManagerImpl::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(source_info, interface_name,
                          std::move(interface_pipe));
}

void PrefStoreManagerImpl::OnPrefStoreDisconnect(
    PrefValueStore::PrefStoreType type) {
  DVLOG(1) << "Deregistering pref store: " << type;
  pref_store_ptrs_.erase(type);
}

void PrefStoreManagerImpl::OnPersistentPrefStoreReady() {
  DVLOG(1) << "PersistentPrefStore ready";
  for (const auto& connection : pending_persistent_connections_)
    connection->ProvidePersistentPrefStore(persistent_pref_store_.get());
  pending_persistent_connections_.clear();
}

bool PrefStoreManagerImpl::Initialized() const {
  return bool(persistent_pref_store_);
}

}  // namespace prefs
