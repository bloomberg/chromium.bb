// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_FLOSS_FLOSS_MANAGER_CLIENT_H_
#define DEVICE_BLUETOOTH_FLOSS_FLOSS_MANAGER_CLIENT_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "dbus/exported_object.h"
#include "dbus/object_manager.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/floss/floss_dbus_client.h"

namespace dbus {
class ErrorResponse;
class PropertySet;
class Response;
}  // namespace dbus

namespace floss {

// The adapter manager client observes the Manager interface which lists all
// available adapters and signals when adapters become available + their state
// changes.
//
// Only the manager interface implements ObjectManager so it should be the first
// point of interaction with the platform adapters. It will also be used to
// select the default adapter.
class DEVICE_BLUETOOTH_EXPORT FlossManagerClient
    : public FlossDBusClient,
      public dbus::ObjectManager::Interface {
 public:
  class Observer : public base::CheckedObserver {
   public:
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    Observer() = default;
    ~Observer() override = default;

    virtual void ManagerPresent(bool present) {}
    virtual void AdapterPresent(int adapter, bool present) {}
    virtual void AdapterEnabledChanged(int adapter, bool enabled) {}
  };

  // Convert adapter number to object path.
  static dbus::ObjectPath GenerateAdapterPath(int adapter);

  // Creates the instance.
  static std::unique_ptr<FlossManagerClient> Create();

  FlossManagerClient(const FlossManagerClient&) = delete;
  FlossManagerClient& operator=(const FlossManagerClient&) = delete;

  FlossManagerClient();
  ~FlossManagerClient() override;

  // Add observers on this client.
  void AddObserver(Observer* observer);

  // Remove observers on this client.
  void RemoveObserver(Observer* observer);

  // Get a list of adapters available on the system.
  virtual std::vector<int> GetAdapters() const;

  // Get the default adapter (index) to use.
  virtual int GetDefaultAdapter() const;

  // Check whether the given adapter is present on the system.
  virtual bool GetAdapterPresent(int adapter) const;

  // Enable or disable Floss at the platform level. This will only be used while
  // Floss is being developed behind a feature flag. This api will be called on
  // the manager to stop Bluez from running and let Floss manage the adapters
  // instead.
  virtual void SetFlossEnabled(bool enable);

  // Check whether an adapter is enabled.
  virtual bool GetAdapterEnabled(int adapter) const;

  // Enable or disable an adapter.
  virtual void SetAdapterEnabled(int adapter,
                                 bool enabled,
                                 ResponseCallback<Void> callback);

  // Initializes the manager client.
  void Init(dbus::Bus* bus,
            const std::string& service_name,
            const std::string& adapter_path) override;

 protected:
  friend class FlossManagerClientTest;

  // Handle response to |GetAvailableAdapters| DBus method call.
  virtual void HandleGetAvailableAdapters(dbus::Response* response,
                                          dbus::ErrorResponse* error);

  // Handle callback |OnHciDeviceChange| on exported object path.
  virtual void OnHciDeviceChange(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  // Handle callback |OnHciEnabledChange| on exported object path.
  virtual void OnHciEnabledChange(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  // Get active adapters and register for callbacks with manager object.
  void RegisterWithManager();

  // Remove active adapters after manager is no longer available.
  void RemoveManager();

  // dbus::ObjectManager::Interface overrides
  dbus::PropertySet* CreateProperties(
      dbus::ObjectProxy* object_proxy,
      const dbus::ObjectPath& object_path,
      const std::string& interface_name) override;

  void ObjectAdded(const dbus::ObjectPath& object_path,
                   const std::string& interface_name) override;
  void ObjectRemoved(const dbus::ObjectPath& object_path,
                     const std::string& interface_name) override;

  // Managed by FlossDBusManager - we keep local pointer to access object proxy.
  dbus::Bus* bus_ = nullptr;

  // Keep track of the object manager so we can keep track of when the manager
  // disappears. Managed by the bus object (do not delete).
  dbus::ObjectManager* object_manager_ = nullptr;

  // Is there a manager available?
  bool manager_available_ = false;

  // Default adapter to use.
  // TODO(b/191906229) - Default adapter should be taken via manager api.
  int default_adapter_ = 0;

  // Cached list of available adapters and their powered state indexed by hci
  // index.
  base::flat_map<int, bool> adapter_to_powered_;

  // Name of service that implements manager interface.
  std::string service_name_;

  // List of observers interested in event notifications from this client.
  base::ObserverList<Observer> observers_;

 private:
  // Object path for exported callbacks registered against manager interface.
  static const char kExportedCallbacksPath[];

  // Floss Manager registers ObjectManager at this path.
  static const char kObjectManagerPath[];

  base::WeakPtrFactory<FlossManagerClient> weak_ptr_factory_{this};
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_FLOSS_MANAGER_CLIENT_H_
