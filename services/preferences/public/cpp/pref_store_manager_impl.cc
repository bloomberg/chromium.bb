// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/public/cpp/pref_store_manager_impl.h"

#include "base/memory/ref_counted.h"
#include "components/prefs/pref_value_store.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/service_manager/public/cpp/interface_registry.h"

namespace prefs {

using ConnectCallback = mojom::PrefStoreConnector::ConnectCallback;
using PrefStorePtrs =
    std::unordered_map<PrefValueStore::PrefStoreType, mojom::PrefStorePtr>;

// Used to make sure all pref stores have been registered before replying to any
// Connect calls.
class ConnectionBarrier : public base::RefCounted<ConnectionBarrier> {
 public:
  static void Create(const PrefStorePtrs& pref_store_ptrs,
                     const ConnectCallback& callback);

  void Init(const PrefStorePtrs& pref_store_ptrs);

 private:
  friend class base::RefCounted<ConnectionBarrier>;
  ConnectionBarrier(const PrefStorePtrs& pref_store_ptrs,
                    const ConnectCallback& callback);
  ~ConnectionBarrier() = default;

  void OnConnect(PrefValueStore::PrefStoreType type,
                 mojom::PrefStoreConnectionPtr connection_ptr);

  ConnectCallback callback_;

  std::unordered_map<PrefValueStore::PrefStoreType,
                     mojom::PrefStoreConnectionPtr>
      connections_;

  const size_t expected_connections_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionBarrier);
};

// static
void ConnectionBarrier::Create(const PrefStorePtrs& pref_store_ptrs,
                               const ConnectCallback& callback) {
  make_scoped_refptr(new ConnectionBarrier(pref_store_ptrs, callback))
      ->Init(pref_store_ptrs);
}

void ConnectionBarrier::Init(const PrefStorePtrs& pref_store_ptrs) {
  for (const auto& ptr : pref_store_ptrs) {
    ptr.second->AddObserver(
        base::Bind(&ConnectionBarrier::OnConnect, this, ptr.first));
  }
}

ConnectionBarrier::ConnectionBarrier(const PrefStorePtrs& pref_store_ptrs,
                                     const ConnectCallback& callback)
    : callback_(callback), expected_connections_(pref_store_ptrs.size()) {}

void ConnectionBarrier::OnConnect(
    PrefValueStore::PrefStoreType type,
    mojom::PrefStoreConnectionPtr connection_ptr) {
  connections_.insert(std::make_pair(type, std::move(connection_ptr)));
  if (connections_.size() == expected_connections_) {
    // After this method exits |this| will get destroyed so it's safe to move
    // out the map.
    callback_.Run(std::move(connections_));
  }
}

PrefStoreManagerImpl::PrefStoreManagerImpl(PrefStoreTypes expected_pref_stores)
    : expected_pref_stores_(std::move(expected_pref_stores)) {}

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
    for (const auto& callback : pending_callbacks_)
      ConnectionBarrier::Create(pref_store_ptrs_, callback);
    pending_callbacks_.clear();
  }
}

void PrefStoreManagerImpl::Connect(const ConnectCallback& callback) {
  if (AllConnected())
    ConnectionBarrier::Create(pref_store_ptrs_, callback);
  else
    pending_callbacks_.push_back(callback);
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

void PrefStoreManagerImpl::OnStart() {}

bool PrefStoreManagerImpl::OnConnect(
    const service_manager::ServiceInfo& remote_info,
    service_manager::InterfaceRegistry* registry) {
  registry->AddInterface<prefs::mojom::PrefStoreConnector>(this);
  registry->AddInterface<prefs::mojom::PrefStoreRegistry>(this);
  return true;
}

void PrefStoreManagerImpl::OnPrefStoreDisconnect(
    PrefValueStore::PrefStoreType type) {
  DVLOG(1) << "Deregistering pref store: " << type;
  pref_store_ptrs_.erase(type);
}

bool PrefStoreManagerImpl::AllConnected() const {
  return pref_store_ptrs_.size() == expected_pref_stores_.size();
}

}  // namespace prefs
