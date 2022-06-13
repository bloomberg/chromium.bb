// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_USERDATAAUTH_INSTALL_ATTRIBUTES_CLIENT_H_
#define CHROMEOS_DBUS_USERDATAAUTH_INSTALL_ATTRIBUTES_CLIENT_H_

#include "base/callback.h"
#include "base/component_export.h"
#include "base/observer_list_types.h"
#include "chromeos/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace dbus {
class Bus;
}

namespace chromeos {

// InstallAttributesClient is used to communicate with the
// org.chromium.InstallAttributes interface within org.chromium.UserDataAuth
// service exposed by cryptohomed. All method should be called from the origin
// thread (UI thread) which initializes the DBusThreadManager instance.
class COMPONENT_EXPORT(USERDATAAUTH_CLIENT) InstallAttributesClient {
 public:
  using InstallAttributesGetCallback =
      DBusMethodCallback<::user_data_auth::InstallAttributesGetReply>;
  using InstallAttributesSetCallback =
      DBusMethodCallback<::user_data_auth::InstallAttributesSetReply>;
  using InstallAttributesFinalizeCallback =
      DBusMethodCallback<::user_data_auth::InstallAttributesFinalizeReply>;
  using InstallAttributesGetStatusCallback =
      DBusMethodCallback<::user_data_auth::InstallAttributesGetStatusReply>;
  using RemoveFirmwareManagementParametersCallback = DBusMethodCallback<
      ::user_data_auth::RemoveFirmwareManagementParametersReply>;
  using SetFirmwareManagementParametersCallback = DBusMethodCallback<
      ::user_data_auth::SetFirmwareManagementParametersReply>;

  // Not copyable or movable.
  InstallAttributesClient(const InstallAttributesClient&) = delete;
  InstallAttributesClient& operator=(const InstallAttributesClient&) = delete;

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance which may be null if not initialized.
  static InstallAttributesClient* Get();

  // Actual DBus Methods:

  // Runs the callback as soon as the service becomes available.
  virtual void WaitForServiceToBeAvailable(
      WaitForServiceToBeAvailableCallback callback) = 0;

  // Retrieves an install attribute.
  virtual void InstallAttributesGet(
      const ::user_data_auth::InstallAttributesGetRequest& request,
      InstallAttributesGetCallback callback) = 0;

  // Set an install attribute.
  virtual void InstallAttributesSet(
      const ::user_data_auth::InstallAttributesSetRequest& request,
      InstallAttributesSetCallback callback) = 0;

  // Finalizes the install attribute.
  virtual void InstallAttributesFinalize(
      const ::user_data_auth::InstallAttributesFinalizeRequest& request,
      InstallAttributesFinalizeCallback callback) = 0;

  // Get the current status of the install attributes.
  virtual void InstallAttributesGetStatus(
      const ::user_data_auth::InstallAttributesGetStatusRequest& request,
      InstallAttributesGetStatusCallback callback) = 0;

  // Removes/unset the firmware management parameters.
  virtual void RemoveFirmwareManagementParameters(
      const ::user_data_auth::RemoveFirmwareManagementParametersRequest&
          request,
      RemoveFirmwareManagementParametersCallback callback) = 0;

  // Set the firmware management parameters.
  virtual void SetFirmwareManagementParameters(
      const ::user_data_auth::SetFirmwareManagementParametersRequest& request,
      SetFirmwareManagementParametersCallback callback) = 0;

  // Blocking version of InstallAttributesGet().
  virtual absl::optional<::user_data_auth::InstallAttributesGetReply>
  BlockingInstallAttributesGet(
      const ::user_data_auth::InstallAttributesGetRequest& request) = 0;

  // Blocking version of InstallAttributesSet().
  virtual absl::optional<::user_data_auth::InstallAttributesSetReply>
  BlockingInstallAttributesSet(
      const ::user_data_auth::InstallAttributesSetRequest& request) = 0;

  // Blocking version of InstallAttributesFinalize().
  virtual absl::optional<::user_data_auth::InstallAttributesFinalizeReply>
  BlockingInstallAttributesFinalize(
      const ::user_data_auth::InstallAttributesFinalizeRequest& request) = 0;

  // Blocking version of InstallAttributesGetStatus().
  virtual absl::optional<::user_data_auth::InstallAttributesGetStatusReply>
  BlockingInstallAttributesGetStatus(
      const ::user_data_auth::InstallAttributesGetStatusRequest& request) = 0;

 protected:
  // Initialize/Shutdown should be used instead.
  InstallAttributesClient();
  virtual ~InstallAttributesClient();
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::InstallAttributesClient;
}

#endif  // CHROMEOS_DBUS_USERDATAAUTH_INSTALL_ATTRIBUTES_CLIENT_H_
