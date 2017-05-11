// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/pref_store_manager_impl.h"

#include <algorithm>
#include <utility>

#include "base/memory/ref_counted.h"
#include "base/stl_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "components/prefs/default_pref_store.h"
#include "components/prefs/pref_value_store.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/preferences/persistent_pref_store_factory.h"
#include "services/preferences/persistent_pref_store_impl.h"
#include "services/preferences/public/cpp/pref_store_impl.h"
#include "services/service_manager/public/cpp/bind_source_info.h"

namespace prefs {
namespace {

using ConnectCallback = mojom::PrefStoreConnector::ConnectCallback;
using PrefStorePtrs =
    std::unordered_map<PrefValueStore::PrefStoreType, mojom::PrefStore*>;

// Used to make sure all pref stores have been registered before replying to any
// Connect calls.
class ConnectionBarrier : public base::RefCounted<ConnectionBarrier> {
 public:
  static void Create(
      const PrefStorePtrs& pref_store_ptrs,
      mojom::PersistentPrefStoreConnectionPtr persistent_pref_store_connection,
      const std::vector<std::string>& observed_prefs,
      const ConnectCallback& callback);

  void Init(const PrefStorePtrs& pref_store_ptrs,
            const std::vector<std::string>& observed_prefs);

 private:
  friend class base::RefCounted<ConnectionBarrier>;
  ConnectionBarrier(
      const PrefStorePtrs& pref_store_ptrs,
      mojom::PersistentPrefStoreConnectionPtr persistent_pref_store_connection,
      const ConnectCallback& callback);
  ~ConnectionBarrier() = default;

  void OnConnect(PrefValueStore::PrefStoreType type,
                 mojom::PrefStoreConnectionPtr connection_ptr);

  ConnectCallback callback_;

  std::unordered_map<PrefValueStore::PrefStoreType,
                     mojom::PrefStoreConnectionPtr>
      connections_;

  const size_t expected_connections_;

  mojom::PersistentPrefStoreConnectionPtr persistent_pref_store_connection_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionBarrier);
};

// static
void ConnectionBarrier::Create(
    const PrefStorePtrs& pref_store_ptrs,
    mojom::PersistentPrefStoreConnectionPtr persistent_pref_store_connection,
    const std::vector<std::string>& observed_prefs,
    const ConnectCallback& callback) {
  make_scoped_refptr(new ConnectionBarrier(
                         pref_store_ptrs,
                         std::move(persistent_pref_store_connection), callback))
      ->Init(pref_store_ptrs, observed_prefs);
}

void ConnectionBarrier::Init(const PrefStorePtrs& pref_store_ptrs,
                             const std::vector<std::string>& observed_prefs) {
  if (pref_store_ptrs.empty()) {
    // After this method exits |this| will get destroyed so it's safe to move
    // out the members.
    callback_.Run(std::move(persistent_pref_store_connection_),
                  std::move(connections_));
    return;
  }
  for (const auto& ptr : pref_store_ptrs) {
    ptr.second->AddObserver(
        observed_prefs,
        base::Bind(&ConnectionBarrier::OnConnect, this, ptr.first));
  }
}

ConnectionBarrier::ConnectionBarrier(
    const PrefStorePtrs& pref_store_ptrs,
    mojom::PersistentPrefStoreConnectionPtr persistent_pref_store_connection,
    const ConnectCallback& callback)
    : callback_(callback),
      expected_connections_(pref_store_ptrs.size()),
      persistent_pref_store_connection_(
          std::move(persistent_pref_store_connection)) {}

void ConnectionBarrier::OnConnect(
    PrefValueStore::PrefStoreType type,
    mojom::PrefStoreConnectionPtr connection_ptr) {
  connections_.insert(std::make_pair(type, std::move(connection_ptr)));
  if (connections_.size() == expected_connections_) {
    // After this method exits |this| will get destroyed so it's safe to move
    // out the members.
    callback_.Run(std::move(persistent_pref_store_connection_),
                  std::move(connections_));
  }
}

}  // namespace

PrefStoreManagerImpl::PrefStoreManagerImpl(
    std::set<PrefValueStore::PrefStoreType> expected_pref_stores,
    scoped_refptr<base::SequencedWorkerPool> worker_pool)
    : expected_pref_stores_(std::move(expected_pref_stores)),
      init_binding_(this),
      defaults_(new DefaultPrefStore),
      defaults_wrapper_(base::MakeUnique<PrefStoreImpl>(
          defaults_,
          mojo::MakeRequest(&pref_store_ptrs_[PrefValueStore::DEFAULT_STORE]))),
      worker_pool_(std::move(worker_pool)) {
  DCHECK(
      base::ContainsValue(expected_pref_stores_, PrefValueStore::USER_STORE) &&
      base::ContainsValue(expected_pref_stores_, PrefValueStore::DEFAULT_STORE))
      << "expected_pref_stores must always include PrefValueStore::USER_STORE "
         "and PrefValueStore::DEFAULT_STORE.";
  // The user store is not actually connected to in the implementation, but
  // accessed directly.
  expected_pref_stores_.erase(PrefValueStore::USER_STORE);
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

PrefStoreManagerImpl::~PrefStoreManagerImpl() = default;

struct PrefStoreManagerImpl::PendingConnect {
  mojom::PrefRegistryPtr pref_registry;
  // Pref stores the caller already is connected to (and hence we won't
  // connect to these).
  std::vector<PrefValueStore::PrefStoreType> already_connected_types;
  ConnectCallback callback;
};

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
  const bool success =
      pref_store_ptrs_.insert(std::make_pair(type, std::move(pref_store_ptr)))
          .second;
  DCHECK(success) << "The same pref store registered twice: " << type;
  if (AllConnected()) {
    DVLOG(1) << "All pref stores registered.";
    ProcessPendingConnects();
  }
}

void PrefStoreManagerImpl::Connect(
    mojom::PrefRegistryPtr pref_registry,
    const std::vector<PrefValueStore::PrefStoreType>& already_connected_types,
    const ConnectCallback& callback) {
  if (AllConnected()) {
    ConnectImpl(std::move(pref_registry), already_connected_types, callback);
  } else {
    pending_connects_.push_back(
        {std::move(pref_registry), already_connected_types, callback});
  }
}

void PrefStoreManagerImpl::BindPrefStoreConnectorRequest(
    const service_manager::BindSourceInfo& source_info,
    prefs::mojom::PrefStoreConnectorRequest request) {
  connector_bindings_.AddBinding(this, std::move(request));
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

  persistent_pref_store_ = CreatePersistentPrefStore(
      std::move(configuration), worker_pool_.get(),
      base::Bind(&PrefStoreManagerImpl::OnPersistentPrefStoreReady,
                 base::Unretained(this)));
  DCHECK(persistent_pref_store_);
  if (AllConnected())
    ProcessPendingConnects();
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

bool PrefStoreManagerImpl::AllConnected() const {
  return pref_store_ptrs_.size() == expected_pref_stores_.size() &&
         persistent_pref_store_ && persistent_pref_store_->initialized();
}

void PrefStoreManagerImpl::ProcessPendingConnects() {
  for (auto& connect : pending_connects_)
    ConnectImpl(std::move(connect.pref_registry),
                connect.already_connected_types, connect.callback);
  pending_connects_.clear();
}

void PrefStoreManagerImpl::ConnectImpl(
    mojom::PrefRegistryPtr pref_registry,
    const std::vector<PrefValueStore::PrefStoreType>& already_connected_types,
    const ConnectCallback& callback) {
  // TODO(crbug.com/719770): Once owning registrations are supported, check the
  // default values in |pref_registry| for consistency with existing defaults
  // and add them to the default pref store.

  // Only connect to pref stores the client isn't already connected to.
  PrefStorePtrs ptrs;
  for (const auto& entry : pref_store_ptrs_) {
    if (!base::ContainsValue(already_connected_types, entry.first)) {
      ptrs.insert(std::make_pair(entry.first, entry.second.get()));
    }
  }
  ConnectionBarrier::Create(ptrs,
                            persistent_pref_store_->CreateConnection(
                                PersistentPrefStoreImpl::ObservedPrefs(
                                    pref_registry->registrations.begin(),
                                    pref_registry->registrations.end())),
                            pref_registry->registrations, callback);
}

void PrefStoreManagerImpl::OnPersistentPrefStoreReady() {
  DVLOG(1) << "PersistentPrefStore ready";
  if (AllConnected()) {
    ProcessPendingConnects();
  }
}

}  // namespace prefs
