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
#include "services/preferences/public/cpp/pref_registry_serializer.h"
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

scoped_refptr<PrefStore> CreatePrefStoreClient(
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

void OnConnect(
    scoped_refptr<RefCountedInterfacePtr<mojom::PrefStoreConnector>>
        connector_ptr,
    scoped_refptr<PrefRegistry> pref_registry,
    ConnectCallback callback,
    mojom::PersistentPrefStoreConnectionPtr persistent_pref_store_connection,
    std::unordered_map<PrefValueStore::PrefStoreType,
                       mojom::PrefStoreConnectionPtr> connections) {
  scoped_refptr<PrefStore> managed_prefs =
      CreatePrefStoreClient(PrefValueStore::MANAGED_STORE, &connections);
  scoped_refptr<PrefStore> supervised_user_prefs = CreatePrefStoreClient(
      PrefValueStore::SUPERVISED_USER_STORE, &connections);
  scoped_refptr<PrefStore> extension_prefs =
      CreatePrefStoreClient(PrefValueStore::EXTENSION_STORE, &connections);
  scoped_refptr<PrefStore> command_line_prefs =
      CreatePrefStoreClient(PrefValueStore::COMMAND_LINE_STORE, &connections);
  scoped_refptr<PrefStore> recommended_prefs =
      CreatePrefStoreClient(PrefValueStore::RECOMMENDED_STORE, &connections);
  // TODO(crbug.com/719770): Once owning registrations are supported, pass the
  // default values of owned prefs to the service and connect to the service's
  // defaults pref store instead of using a local one.
  scoped_refptr<PersistentPrefStore> persistent_pref_store(
      new PersistentPrefStoreClient(
          std::move(persistent_pref_store_connection)));
  PrefNotifierImpl* pref_notifier = new PrefNotifierImpl();
  auto* pref_value_store = new PrefValueStore(
      managed_prefs.get(), supervised_user_prefs.get(), extension_prefs.get(),
      command_line_prefs.get(), persistent_pref_store.get(),
      recommended_prefs.get(), pref_registry->defaults().get(), pref_notifier);
  callback.Run(base::MakeUnique<::PrefService>(
      pref_notifier, pref_value_store, persistent_pref_store.get(),
      pref_registry.get(), base::Bind(&DoNothingHandleReadError), true));
  connector_ptr->reset();
}

void OnConnectError(
    scoped_refptr<RefCountedInterfacePtr<mojom::PrefStoreConnector>>
        connector_ptr,
    ConnectCallback callback) {
  callback.Run(nullptr);
  connector_ptr->reset();
}

}  // namespace

void ConnectToPrefService(
    service_manager::Connector* connector,
    scoped_refptr<PrefRegistry> pref_registry,
    std::vector<PrefValueStore::PrefStoreType> already_connected_types,
    ConnectCallback callback,
    base::StringPiece service_name) {
  already_connected_types.push_back(PrefValueStore::DEFAULT_STORE);
  auto connector_ptr = make_scoped_refptr(
      new RefCountedInterfacePtr<mojom::PrefStoreConnector>());
  connector->BindInterface(service_name.as_string(), &connector_ptr->get());
  connector_ptr->get().set_connection_error_handler(base::Bind(
      &OnConnectError, connector_ptr, base::Passed(ConnectCallback{callback})));
  auto serialized_pref_registry = SerializePrefRegistry(*pref_registry);
  connector_ptr->get()->Connect(
      std::move(serialized_pref_registry), std::move(already_connected_types),
      base::Bind(&OnConnect, connector_ptr, base::Passed(&pref_registry),
                 base::Passed(&callback)));
}

}  // namespace prefs
