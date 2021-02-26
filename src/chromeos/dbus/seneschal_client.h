// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SENESCHAL_CLIENT_H_
#define CHROMEOS_DBUS_SENESCHAL_CLIENT_H_

#include "base/component_export.h"
#include "base/observer_list.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/seneschal/seneschal_service.pb.h"
#include "dbus/object_proxy.h"

namespace chromeos {

// SeneschalClient is used to communicate with Seneschal, which manages
// 9p file servers.
class COMPONENT_EXPORT(CHROMEOS_DBUS) SeneschalClient : public DBusClient {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when Seneschal service exits.
    virtual void SeneschalServiceStopped() = 0;
    // Called when Seneschal service is either started or restarted.
    virtual void SeneschalServiceStarted() = 0;
  };

  // Adds an observer.
  virtual void AddObserver(Observer* observer) = 0;

  // Removes an observer if added.
  virtual void RemoveObserver(Observer* observer) = 0;

  ~SeneschalClient() override;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static std::unique_ptr<SeneschalClient> Create();

  // Registers |callback| to run when the Concierge service becomes available.
  // If the service is already available, or if connecting to the name-owner-
  // changed signal fails, |callback| will be run once asynchronously.
  // Otherwise, |callback| will be run once in the future after the service
  // becomes available.
  virtual void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) = 0;

  // Shares a path in the Chrome OS host with the container.
  // |callback| is called after the method call finishes.
  virtual void SharePath(
      const vm_tools::seneschal::SharePathRequest& request,
      DBusMethodCallback<vm_tools::seneschal::SharePathResponse> callback) = 0;

  // Unshares a path in the Chrome OS host with the container.
  // |callback| is called after the method call finishes.
  virtual void UnsharePath(
      const vm_tools::seneschal::UnsharePathRequest& request,
      DBusMethodCallback<vm_tools::seneschal::UnsharePathResponse>
          callback) = 0;

 protected:
  // Create() should be used instead.
  SeneschalClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(SeneschalClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SENESCHAL_CLIENT_H_
