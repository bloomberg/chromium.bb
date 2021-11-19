// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_FLOSS_FLOSS_DBUS_MANAGER_H_
#define DEVICE_BLUETOOTH_FLOSS_FLOSS_DBUS_MANAGER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_export.h"

namespace base {
class Thread;
}  // namespace base

namespace dbus {
class Bus;
class Response;
class ErrorResponse;
}  // namespace dbus

namespace floss {

class FlossAdapterClient;
class FlossManagerClient;
class FlossClientBundle;
class FlossDBusManagerSetter;

// FlossDBusManager manages the lifetimes of D-Bus connections and clients. It
// ensures the proper ordering of shutdowns for the D-Bus thread, connections
// and clients.
//
// While similar to the BluezDBusManager, it requires totally different client
// implementations since the Floss dbus apis are drastically different. Thus, it
// doesn't make sense to share a common implementation between the two.
class DEVICE_BLUETOOTH_EXPORT FlossDBusManager {
 public:
  // Initializes the global instance with a real client. Must be called before
  // any calls to Get(). We explicitly initialize and shutdown the global object
  // rather than making it a Singleton to ensure clean startup and shutdown.
  static void Initialize(dbus::Bus* system_bus);

  // Initializes the global instance with a fake client
  // TODO(b/193712484) - Implement this. Only added here to fix debug build.
  static void InitializeFake();

  // Returns a FlossDBusManagerSetter instance that allows tests to
  // replace individual D-Bus clients with their own implementations.
  // Also initializes the main FlossDBusManager for testing if necessary.
  static std::unique_ptr<FlossDBusManagerSetter> GetSetterForTesting();

  // Returns true if FlossDBusManager has been initialized. Call this to
  // avoid initializing + shutting down FlossDBusManager more than once.
  static bool IsInitialized();

  // Destroys the global instance. This will clean up all the clients managed by
  // the global instance.
  static void Shutdown();

  // Gets the global instance. Initialize() must be called first.
  static FlossDBusManager* Get();

  // Adapters are indexed from 0-N (corresponding to hci0..hciN). Use negative
  // value to represent invalid adapter.
  static const int kInvalidAdapter;

  FlossDBusManager(const FlossDBusManager&) = delete;
  FlossDBusManager& operator=(const FlossDBusManager&) = delete;

  // Calls |callback| once we know whether Object Manager is supported or not.
  // If the object manager support is already known when this is called,
  // |callback| is run right away and this function returns true. Otherwise, it
  // will return false.
  bool CallWhenObjectManagerSupportIsKnown(base::OnceClosure callback);

  // Returns true if Object Manager is support is known.
  bool IsObjectManagerSupportKnown() const {
    return object_manager_support_known_;
  }

  // Returns true if Object Manager is supported.
  bool IsObjectManagerSupported() const { return object_manager_supported_; }

  // Shuts down the existing adapter clients and initializes a new set for the
  // given adapter.
  void SwitchAdapter(int adapter);

  // Checks whether an adapter is currently enabled and being used.
  bool HasActiveAdapter() const;

  // Returns system bus pointer (owned by FlossDBusThreadManager).
  dbus::Bus* GetSystemBus() const { return bus_; }

  // All returned objects are owned by FlossDBusManager. Do not use these
  // pointers after FlossDBusManager has been shut down.
  FlossManagerClient* GetManagerClient();
  FlossAdapterClient* GetAdapterClient();

 private:
  friend class FlossDBusManagerSetter;

  // Creates a global instance of FlossDBusManager. Cannot be called more than
  // once.
  static void CreateGlobalInstance(dbus::Bus* bus, bool use_stubs);

  FlossDBusManager(dbus::Bus* bus, bool use_stubs);
  ~FlossDBusManager();

  void OnObjectManagerSupported(dbus::Response* response);
  void OnObjectManagerNotSupported(dbus::ErrorResponse* response);

  // Initializes the manager client
  void InitializeManagerClient();

  // Initializes all currently stored DBusClients with the system bus and
  // performs additional setup for a specific adapter.
  void InitializeAdapterClients(int adapter);

  // System bus instance (owned by FlossDBusThreadManager).
  dbus::Bus* bus_;

  // Bundle together all Floss clients to be initialized and shutdown in
  // a specified order.
  std::unique_ptr<FlossClientBundle> client_bundle_ = nullptr;

  // FlossDBusManager depends on object manager support to know when the manager
  // object has been added (via the InterfaceAdded signal) so it can detect if
  // an adapter is present. This callback gets called after GetManagedObjects
  // returns a response (an array or an error).
  base::OnceClosure object_manager_support_known_callback_;
  bool object_manager_support_known_ = false;
  bool object_manager_supported_ = false;

  // Currently active Bluetooth adapter
  int active_adapter_ = kInvalidAdapter;

  base::WeakPtrFactory<FlossDBusManager> weak_ptr_factory_{this};
};

class DEVICE_BLUETOOTH_EXPORT FlossDBusManagerSetter {
 public:
  void SetFlossManagerClient(std::unique_ptr<FlossManagerClient> client);
  void SetFlossAdapterClient(std::unique_ptr<FlossAdapterClient> client);
};

// FlossDBusThreadManager manages the D-Bus thread, the thread dedicated to
// handling asynchronous D-Bus operations.
class DEVICE_BLUETOOTH_EXPORT FlossDBusThreadManager {
 public:
  FlossDBusThreadManager(const FlossDBusThreadManager&) = delete;
  FlossDBusThreadManager& operator=(const FlossDBusThreadManager&) = delete;

  // Sets the global instance. Must be called before any calls to Get().
  // We explicitly initialize and shut down the global object, rather than
  // making it a Singleton, to ensure clean startup and shutdown.
  static void Initialize();

  // Destroys the global instance.
  static void Shutdown();

  // Gets the global instance. Initialize() must be called first.
  static FlossDBusThreadManager* Get();

  // Returns system bus pointer (owned by FlossDBusThreadManager).
  dbus::Bus* GetSystemBus();

 private:
  FlossDBusThreadManager();
  ~FlossDBusThreadManager();

  std::unique_ptr<base::Thread> dbus_thread_;
  scoped_refptr<dbus::Bus> system_bus_;
};

// The bundle of all DBus clients used by FlossDBusManager. The bundle is used
// to control the correct ordering of creation and deletion of clients before
// shutting down the system bus.
class DEVICE_BLUETOOTH_EXPORT FlossClientBundle {
 public:
  FlossClientBundle(const FlossClientBundle&) = delete;
  FlossClientBundle& operator=(const FlossClientBundle&) = delete;

  explicit FlossClientBundle(bool use_stubs);
  ~FlossClientBundle();

  FlossManagerClient* manager_client() { return manager_client_.get(); }

  FlossAdapterClient* adapter_client() { return adapter_client_.get(); }

 private:
  friend FlossDBusManagerSetter;
  friend FlossDBusManager;

  void ResetAdapterClients();

  bool use_stubs_;
  std::unique_ptr<FlossManagerClient> manager_client_;
  std::unique_ptr<FlossAdapterClient> adapter_client_;
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_FLOSS_DBUS_MANAGER_H_
