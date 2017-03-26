// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/pref_store_manager_impl.h"

#include <utility>

#include "base/memory/ref_counted.h"
#include "base/threading/sequenced_worker_pool.h"
#include "components/prefs/pref_value_store.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/preferences/persistent_pref_store_factory.h"
#include "services/preferences/persistent_pref_store_impl.h"
#include "services/service_manager/public/cpp/interface_registry.h"

namespace prefs {
namespace {

using ConnectCallback = mojom::PrefStoreConnector::ConnectCallback;
using PrefStorePtrs =
    std::unordered_map<PrefValueStore::PrefStoreType, mojom::PrefStorePtr>;

// Used to make sure all pref stores have been registered before replying to any
// Connect calls.
class ConnectionBarrier : public base::RefCounted<ConnectionBarrier> {
 public:
  static void Create(
      const PrefStorePtrs& pref_store_ptrs,
      mojom::PersistentPrefStoreConnectionPtr persistent_pref_store_connection,
      const ConnectCallback& callback);

  void Init(const PrefStorePtrs& pref_store_ptrs);

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
    const ConnectCallback& callback) {
  make_scoped_refptr(new ConnectionBarrier(
                         pref_store_ptrs,
                         std::move(persistent_pref_store_connection), callback))
      ->Init(pref_store_ptrs);
}

void ConnectionBarrier::Init(const PrefStorePtrs& pref_store_ptrs) {
  if (expected_connections_ == 0) {
    // Degenerate case. We don't expect this, but it could happen in
    // e.g. testing.
    callback_.Run(std::move(persistent_pref_store_connection_),
                  std::move(connections_));
    return;
  }
  for (const auto& ptr : pref_store_ptrs) {
    ptr.second->AddObserver(
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
      worker_pool_(std::move(worker_pool)) {}

PrefStoreManagerImpl::~PrefStoreManagerImpl() = default;

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

void PrefStoreManagerImpl::Connect(const ConnectCallback& callback) {
  if (AllConnected()) {
    ConnectImpl(callback);
  } else {
    pending_callbacks_.push_back(callback);
  }
}

void PrefStoreManagerImpl::Create(
    const service_manager::Identity& remote_identity,
    prefs::mojom::PrefStoreConnectorRequest request) {
  connector_bindings_.AddBinding(this, std::move(request));
}

void PrefStoreManagerImpl::Create(
    const service_manager::Identity& remote_identity,
    prefs::mojom::PrefStoreRegistryRequest request) {
  registry_bindings_.AddBinding(this, std::move(request));
}

void PrefStoreManagerImpl::Create(
    const service_manager::Identity& remote_identity,
    prefs::mojom::PrefServiceControlRequest request) {
  if (init_binding_.is_bound()) {
    LOG(ERROR)
        << "Pref service received unexpected control interface connection from "
        << remote_identity.name();
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

bool PrefStoreManagerImpl::OnConnect(
    const service_manager::ServiceInfo& remote_info,
    service_manager::InterfaceRegistry* registry) {
  registry->AddInterface<prefs::mojom::PrefStoreConnector>(this);
  registry->AddInterface<prefs::mojom::PrefStoreRegistry>(this);
  registry->AddInterface<prefs::mojom::PrefServiceControl>(this);
  return true;
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
  for (auto& connect : pending_callbacks_)
    ConnectImpl(connect);
  pending_callbacks_.clear();
}

void PrefStoreManagerImpl::ConnectImpl(const ConnectCallback& callback) {
  ConnectionBarrier::Create(
      pref_store_ptrs_, persistent_pref_store_->CreateConnection(), callback);
}

void PrefStoreManagerImpl::OnPersistentPrefStoreReady() {
  DVLOG(1) << "PersistentPrefStore ready";
  if (AllConnected()) {
    ProcessPendingConnects();
  }
}

}  // namespace prefs
