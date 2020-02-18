// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SHILL_SHILL_IPCONFIG_CLIENT_H_
#define CHROMEOS_DBUS_SHILL_SHILL_IPCONFIG_CLIENT_H_

#include <string>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "chromeos/dbus/shill/shill_client_helper.h"

namespace base {
class Value;
class DictionaryValue;
}  // namespace base

namespace dbus {
class Bus;
class ObjectPath;
}  // namespace dbus

namespace chromeos {

class ShillPropertyChangedObserver;

// ShillIPConfigClient is used to communicate with the Shill IPConfig
// service.  All methods should be called from the origin thread which
// initializes the DBusThreadManager instance.
class COMPONENT_EXPORT(SHILL_CLIENT) ShillIPConfigClient {
 public:
  typedef ShillClientHelper::DictionaryValueCallback DictionaryValueCallback;

  class TestInterface {
   public:
    // Adds an IPConfig entry.
    virtual void AddIPConfig(const std::string& ip_config_path,
                             const base::DictionaryValue& properties) = 0;

   protected:
    virtual ~TestInterface() {}
  };

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates the global instance with a fake implementation.
  static void InitializeFake();

  // Destroys the global instance which must have been initialized.
  static void Shutdown();

  // Returns the global instance if initialized. May return null.
  static ShillIPConfigClient* Get();

  // Factory function, creates a new instance which is owned by the caller.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static ShillIPConfigClient* Create();

  // Adds a property changed |observer| for the ipconfig at |ipconfig_path|.
  virtual void AddPropertyChangedObserver(
      const dbus::ObjectPath& ipconfig_path,
      ShillPropertyChangedObserver* observer) = 0;

  // Removes a property changed |observer| for the ipconfig at |ipconfig_path|.
  virtual void RemovePropertyChangedObserver(
      const dbus::ObjectPath& ipconfig_path,
      ShillPropertyChangedObserver* observer) = 0;

  // Calls GetProperties method.
  // |callback| is called after the method call succeeds.
  virtual void GetProperties(const dbus::ObjectPath& ipconfig_path,
                             DictionaryValueCallback callback) = 0;

  // Calls SetProperty method.
  // |callback| is called after the method call succeeds.
  virtual void SetProperty(const dbus::ObjectPath& ipconfig_path,
                           const std::string& name,
                           const base::Value& value,
                           VoidDBusMethodCallback callback) = 0;

  // Calls ClearProperty method.
  // |callback| is called after the method call succeeds.
  virtual void ClearProperty(const dbus::ObjectPath& ipconfig_path,
                             const std::string& name,
                             VoidDBusMethodCallback callback) = 0;

  // Calls Remove method.
  // |callback| is called after the method call succeeds.
  virtual void Remove(const dbus::ObjectPath& ipconfig_path,
                      VoidDBusMethodCallback callback) = 0;

  // Returns an interface for testing (stub only), or returns null.
  virtual ShillIPConfigClient::TestInterface* GetTestInterface() = 0;

 protected:
  friend class ShillIPConfigClientTest;

  // Initialize/Shutdown should be used instead.
  ShillIPConfigClient();
  virtual ~ShillIPConfigClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(ShillIPConfigClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SHILL_SHILL_IPCONFIG_CLIENT_H_
