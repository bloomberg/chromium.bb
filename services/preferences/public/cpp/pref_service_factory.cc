// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/public/cpp/pref_service_factory.h"

#include "base/callback_helpers.h"
#include "components/prefs/persistent_pref_store.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_value_store.h"
#include "services/preferences/public/cpp/persistent_pref_store_client.h"
#include "services/preferences/public/cpp/pref_store_client.h"
#include "services/preferences/public/interfaces/preferences.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace prefs {
namespace {

// Used to implement a "fire and forget" pattern where we call an interface
// method, with an attached error handler, but don't care to hold on to the
// InterfacePtr after.
template <typename Interface>
class RefCountedInterfacePtr
    : public base::RefCounted<RefCountedInterfacePtr<Interface>> {
 public:
  mojo::InterfacePtr<Interface>& get() { return ptr_; }
  void reset() { ptr_.reset(); }

 private:
  friend class base::RefCounted<RefCountedInterfacePtr<Interface>>;
  ~RefCountedInterfacePtr() = default;

  mojo::InterfacePtr<Interface> ptr_;
};

void DoNothingHandleReadError(PersistentPrefStore::PrefReadError error) {}

class ConnectionBarrier : public base::RefCounted<ConnectionBarrier> {
 public:
  static void Create(service_manager::Connector* connector,
                     scoped_refptr<PrefRegistry> pref_registry,
                     ConnectCallback callback);

 private:
  friend class base::RefCounted<ConnectionBarrier>;
  ConnectionBarrier(scoped_refptr<PrefRegistry> pref_registry,
                    scoped_refptr<PersistentPrefStore> persistent_pref_store,
                    ConnectCallback callback);
  ~ConnectionBarrier() = default;

  void OnConnect(
      scoped_refptr<RefCountedInterfacePtr<mojom::PrefStoreConnector>> unused,
      scoped_refptr<PrefRegistry> pref_registry,
      std::unordered_map<PrefValueStore::PrefStoreType,
                         mojom::PrefStoreConnectionPtr> connections);

  void OnConnectError(
      scoped_refptr<RefCountedInterfacePtr<mojom::PrefStoreConnector>> unused);

  scoped_refptr<PrefRegistry> pref_registry_;
  scoped_refptr<PersistentPrefStore> persistent_pref_store_;
  ConnectCallback callback_;
};

scoped_refptr<PrefStore> CreatePrefStore(
    PrefValueStore::PrefStoreType store_type,
    std::unordered_map<PrefValueStore::PrefStoreType,
                       mojom::PrefStoreConnectionPtr>* connections) {
  auto pref_store_it = connections->find(store_type);
  if (pref_store_it != connections->end()) {
    return make_scoped_refptr(
        new PrefStoreClient(std::move(pref_store_it->second)));
  } else {
    return nullptr;
  }
}

ConnectionBarrier::ConnectionBarrier(
    scoped_refptr<PrefRegistry> pref_registry,
    scoped_refptr<PersistentPrefStore> persistent_pref_store,
    ConnectCallback callback)
    : pref_registry_(std::move(pref_registry)),
      persistent_pref_store_(std::move(persistent_pref_store)),
      callback_(std::move(callback)) {}

void ConnectionBarrier::OnConnect(
    scoped_refptr<RefCountedInterfacePtr<mojom::PrefStoreConnector>>
        connector_ptr,
    scoped_refptr<PrefRegistry> pref_registry,
    std::unordered_map<PrefValueStore::PrefStoreType,
                       mojom::PrefStoreConnectionPtr> connections) {
  scoped_refptr<PrefStore> managed_prefs =
      CreatePrefStore(PrefValueStore::MANAGED_STORE, &connections);
  scoped_refptr<PrefStore> supervised_user_prefs =
      CreatePrefStore(PrefValueStore::SUPERVISED_USER_STORE, &connections);
  scoped_refptr<PrefStore> extension_prefs =
      CreatePrefStore(PrefValueStore::EXTENSION_STORE, &connections);
  scoped_refptr<PrefStore> command_line_prefs =
      CreatePrefStore(PrefValueStore::COMMAND_LINE_STORE, &connections);
  scoped_refptr<PrefStore> recommended_prefs =
      CreatePrefStore(PrefValueStore::RECOMMENDED_STORE, &connections);
  scoped_refptr<PrefStore> default_prefs =
      CreatePrefStore(PrefValueStore::DEFAULT_STORE, &connections);
  PrefNotifierImpl* pref_notifier = new PrefNotifierImpl();
  auto* pref_value_store = new PrefValueStore(
      managed_prefs.get(), supervised_user_prefs.get(), extension_prefs.get(),
      command_line_prefs.get(), persistent_pref_store_.get(),
      recommended_prefs.get(), default_prefs.get(), pref_notifier);
  base::ResetAndReturn(&callback_)
      .Run(base::MakeUnique<::PrefService>(
          pref_notifier, pref_value_store, persistent_pref_store_.get(),
          pref_registry_.get(), base::Bind(&DoNothingHandleReadError), true));
  connector_ptr->reset();
}

void ConnectionBarrier::OnConnectError(
    scoped_refptr<RefCountedInterfacePtr<mojom::PrefStoreConnector>>
        connector_ptr) {
  callback_.Run(nullptr);
  connector_ptr->reset();
}

// static
void ConnectionBarrier::Create(service_manager::Connector* connector,
                               scoped_refptr<PrefRegistry> pref_registry,
                               ConnectCallback callback) {
  // Connect to user pref store.
  mojom::PersistentPrefStoreConnectorPtr persistent_connector_ptr;
  connector->BindInterface(mojom::kPrefStoreServiceName,
                           &persistent_connector_ptr);
  auto barrier = make_scoped_refptr(new ConnectionBarrier(
      std::move(pref_registry),
      make_scoped_refptr(
          new PersistentPrefStoreClient(std::move(persistent_connector_ptr))),
      std::move(callback)));

  // Connect to all other pref stores.
  auto connector_ptr = make_scoped_refptr(
      new RefCountedInterfacePtr<mojom::PrefStoreConnector>());
  connector->BindInterface(mojom::kPrefStoreServiceName, &connector_ptr->get());
  connector_ptr->get().set_connection_error_handler(
      base::Bind(&ConnectionBarrier::OnConnectError, barrier, connector_ptr));
  connector_ptr->get()->Connect(base::Bind(&ConnectionBarrier::OnConnect,
                                           barrier, connector_ptr,
                                           std::move(pref_registry)));
}

}  // namespace

void ConnectToPrefService(service_manager::Connector* connector,
                          scoped_refptr<PrefRegistry> pref_registry,
                          const ConnectCallback& callback) {
  ConnectionBarrier::Create(connector, std::move(pref_registry), callback);
}

}  // namespace prefs
