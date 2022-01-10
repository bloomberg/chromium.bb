// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SHILL_SHILL_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_SHILL_SHILL_MANAGER_CLIENT_H_

#include <string>

#include "base/component_export.h"
#include "base/time/time.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/shill/fake_shill_simulated_result.h"
#include "chromeos/dbus/shill/shill_client_helper.h"

namespace dbus {
class Bus;
class ObjectPath;
}  // namespace dbus

namespace chromeos {

class ShillPropertyChangedObserver;

// ShillManagerClient is used to communicate with the Shill Manager
// service.  All methods should be called from the origin thread which
// initializes the DBusThreadManager instance. Most methods that make Shill
// Manager calls pass |callback| which will be invoked if the method call
// succeeds, and |error_callback| which will be invoked if the method call fails
// or returns an error response.
class COMPONENT_EXPORT(SHILL_CLIENT) ShillManagerClient {
 public:
  typedef ShillClientHelper::ErrorCallback ErrorCallback;

  struct NetworkThrottlingStatus {
    // Enable or disable network bandwidth throttling.
    // Following fields are available only if |enabled| is true.
    bool enabled;

    // Uploading rate (kbits/s).
    uint32_t upload_rate_kbits;

    // Downloading rate (kbits/s).
    uint32_t download_rate_kbits;
  };

  // Interface for setting up devices, services, and technologies for testing.
  // Accessed through GetTestInterface(), only implemented in the Stub Impl.
  class TestInterface {
   public:
    virtual void AddDevice(const std::string& device_path) = 0;
    virtual void RemoveDevice(const std::string& device_path) = 0;
    virtual void ClearDevices() = 0;
    virtual void AddTechnology(const std::string& type, bool enabled) = 0;
    virtual void RemoveTechnology(const std::string& type) = 0;
    virtual void SetTechnologyInitializing(const std::string& type,
                                           bool initializing) = 0;
    virtual void SetTechnologyProhibited(const std::string& type,
                                         bool prohibited) = 0;
    // |network| must be a dictionary describing a Shill network configuration
    // which will be appended to the results returned from
    // GetNetworksForGeolocation().
    virtual void AddGeoNetwork(const std::string& technology,
                               const base::Value& network) = 0;

    // Does not create an actual profile in the ProfileClient but update the
    // profiles list and sends a notification to observers. This should only be
    // called by the ProfileStub. In all other cases, use
    // ShillProfileClient::TestInterface::AddProfile.
    virtual void AddProfile(const std::string& profile_path) = 0;

    // Used to reset all properties; does not notify observers.
    virtual void ClearProperties() = 0;

    // Set manager property.
    virtual void SetManagerProperty(const std::string& key,
                                    const base::Value& value) = 0;

    // Modify services in the Manager's list.
    virtual void AddManagerService(const std::string& service_path,
                                   bool notify_observers) = 0;
    virtual void RemoveManagerService(const std::string& service_path) = 0;
    virtual void ClearManagerServices() = 0;

    // Returns all enabled services in the given property.
    virtual base::Value GetEnabledServiceList() const = 0;

    // Called by ShillServiceClient when a service's State property changes,
    // before notifying observers. Sets the DefaultService property to empty
    // if the state changes to a non-connected state.
    virtual void ServiceStateChanged(const std::string& service_path,
                                     const std::string& state) = 0;

    // Called by ShillServiceClient when a service's State or Visibile
    // property changes. If |notify| is true, notifies observers if a list
    // changed. Services are sorted first by active, inactive, or disabled
    // state, then by type.
    virtual void SortManagerServices(bool notify) = 0;

    // Sets up the default fake environment based on default initial states
    // or states provided by the command line.
    virtual void SetupDefaultEnvironment() = 0;

    // Returns the interactive delay (specified by the command line or a test).
    virtual base::TimeDelta GetInteractiveDelay() const = 0;

    // Sets the interactive delay for testing.
    virtual void SetInteractiveDelay(base::TimeDelta delay) = 0;

    // Sets the 'best' service to connect to on a ConnectToBestServices call.
    virtual void SetBestServiceToConnect(const std::string& service_path) = 0;

    // Returns the current network throttling status.
    virtual const NetworkThrottlingStatus& GetNetworkThrottlingStatus() = 0;

    // Returns the current Fast Transition status.
    virtual bool GetFastTransitionStatus() = 0;

    // Makes ConfigureService succeed, fail, or timeout.
    virtual void SetSimulateConfigurationResult(
        FakeShillSimulatedResult configuration_result) = 0;

    // Clears profile list.
    virtual void ClearProfiles() = 0;

    // Set (or unset) stub client state to return nullopt on GetProperties().
    virtual void SetShouldReturnNullProperties(bool value) = 0;

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
  static ShillManagerClient* Get();

  ShillManagerClient(const ShillManagerClient&) = delete;
  ShillManagerClient& operator=(const ShillManagerClient&) = delete;

  // Adds a property changed |observer|.
  virtual void AddPropertyChangedObserver(
      ShillPropertyChangedObserver* observer) = 0;

  // Removes a property changed |observer|.
  virtual void RemovePropertyChangedObserver(
      ShillPropertyChangedObserver* observer) = 0;

  // Calls the GetProperties DBus method and invokes |callback| when complete.
  // |callback| receives a dictionary Value containing the Manager properties on
  // success or nullopt on failure.
  virtual void GetProperties(DBusMethodCallback<base::Value> callback) = 0;

  // Calls the GetNetworksForGeolocation DBus method and invokes |callback| when
  // complete. |callback| receives a dictionary Value containing an entry for
  // available network types. See Shill manager-api documentation for details.
  virtual void GetNetworksForGeolocation(
      DBusMethodCallback<base::Value> callback) = 0;

  // Calls SetProperty method.
  virtual void SetProperty(const std::string& name,
                           const base::Value& value,
                           base::OnceClosure callback,
                           ErrorCallback error_callback) = 0;

  // Calls RequestScan method.
  virtual void RequestScan(const std::string& type,
                           base::OnceClosure callback,
                           ErrorCallback error_callback) = 0;

  // Calls EnableTechnology method.
  virtual void EnableTechnology(const std::string& type,
                                base::OnceClosure callback,
                                ErrorCallback error_callback) = 0;

  // Calls DisableTechnology method.
  virtual void DisableTechnology(const std::string& type,
                                 base::OnceClosure callback,
                                 ErrorCallback error_callback) = 0;

  // Calls Manager.ConfigureService with |properties| which must be a
  // dictionary value describing a Shill service.
  virtual void ConfigureService(const base::Value& properties,
                                ObjectPathCallback callback,
                                ErrorCallback error_callback) = 0;

  // Calls Manager.ConfigureServiceForProfile for |profile_path| with
  // |properties| which must be a dictionary value describing a Shill service.
  virtual void ConfigureServiceForProfile(const dbus::ObjectPath& profile_path,
                                          const base::Value& properties,
                                          ObjectPathCallback callback,
                                          ErrorCallback error_callback) = 0;

  // Calls Manager.GetService with |properties| which must be a dictionary value
  // describing a Service.
  virtual void GetService(const base::Value& properties,
                          ObjectPathCallback callback,
                          ErrorCallback error_callback) = 0;

  // For each technology present, connects to the "best" service available.
  // Called once the user is logged in and certificates are loaded.
  virtual void ConnectToBestServices(base::OnceClosure callback,
                                     ErrorCallback error_callback) = 0;

  // Enable or disable network bandwidth throttling, on all interfaces on the
  // system.
  virtual void SetNetworkThrottlingStatus(const NetworkThrottlingStatus& status,
                                          base::OnceClosure callback,
                                          ErrorCallback error_callback) = 0;

  // Creates a set of Passpoint credentials from |properties| in the profile
  // referenced by |profile_path|.
  virtual void AddPasspointCredentials(const dbus::ObjectPath& profile_path,
                                       const base::Value& properties,
                                       base::OnceClosure callback,
                                       ErrorCallback error_callback) = 0;

  // Returns an interface for testing (stub only), or returns null.
  virtual TestInterface* GetTestInterface() = 0;

 protected:
  friend class ShillManagerClientTest;

  // Initialize/Shutdown should be used instead.
  ShillManagerClient();
  virtual ~ShillManagerClient();
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::ShillManagerClient;
}

#endif  // CHROMEOS_DBUS_SHILL_SHILL_MANAGER_CLIENT_H_
