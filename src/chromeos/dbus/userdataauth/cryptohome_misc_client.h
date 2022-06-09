// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_USERDATAAUTH_CRYPTOHOME_MISC_CLIENT_H_
#define CHROMEOS_DBUS_USERDATAAUTH_CRYPTOHOME_MISC_CLIENT_H_

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

// CryptohomeMiscClient is used to communicate with the
// org.chromium.CryptohomeMisc interface within org.chromium.UserDataAuth
// service exposed by cryptohomed. All method should be called from the origin
// thread (UI thread) which initializes the DBusThreadManager instance.
class COMPONENT_EXPORT(USERDATAAUTH_CLIENT) CryptohomeMiscClient {
 public:
  using GetSystemSaltCallback =
      DBusMethodCallback<::user_data_auth::GetSystemSaltReply>;
  using GetSanitizedUsernameCallback =
      DBusMethodCallback<::user_data_auth::GetSanitizedUsernameReply>;
  using GetLoginStatusCallback =
      DBusMethodCallback<::user_data_auth::GetLoginStatusReply>;
  using LockToSingleUserMountUntilRebootCallback = DBusMethodCallback<
      ::user_data_auth::LockToSingleUserMountUntilRebootReply>;
  using GetRsuDeviceIdCallback =
      DBusMethodCallback<::user_data_auth::GetRsuDeviceIdReply>;
  using CheckHealthCallback =
      DBusMethodCallback<::user_data_auth::CheckHealthReply>;

  // Not copyable or movable.
  CryptohomeMiscClient(const CryptohomeMiscClient&) = delete;
  CryptohomeMiscClient& operator=(const CryptohomeMiscClient&) = delete;

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance which may be null if not initialized.
  static CryptohomeMiscClient* Get();

  // Actual DBus Methods:

  // Runs the callback as soon as the service becomes available.
  virtual void WaitForServiceToBeAvailable(
      WaitForServiceToBeAvailableCallback callback) = 0;

  // Retrieves the system salt.
  virtual void GetSystemSalt(
      const ::user_data_auth::GetSystemSaltRequest& request,
      GetSystemSaltCallback callback) = 0;

  // Converts plain username to "hashed" username.
  virtual void GetSanitizedUsername(
      const ::user_data_auth::GetSanitizedUsernameRequest& request,
      GetSanitizedUsernameCallback callback) = 0;

  // Retrieves the login status.
  virtual void GetLoginStatus(
      const ::user_data_auth::GetLoginStatusRequest& request,
      GetLoginStatusCallback callback) = 0;

  // Locks the current boot into single user.
  virtual void LockToSingleUserMountUntilReboot(
      const ::user_data_auth::LockToSingleUserMountUntilRebootRequest& request,
      LockToSingleUserMountUntilRebootCallback callback) = 0;

  // Retrieves the RSU device ID.
  virtual void GetRsuDeviceId(
      const ::user_data_auth::GetRsuDeviceIdRequest& request,
      GetRsuDeviceIdCallback callback) = 0;

  // Returns the "health" state of the system. i.e. If powerwash is needed.
  virtual void CheckHealth(const ::user_data_auth::CheckHealthRequest& request,
                           CheckHealthCallback callback) = 0;

  // Blocking version of GetSanitizedUsername().
  virtual absl::optional<::user_data_auth::GetSanitizedUsernameReply>
  BlockingGetSanitizedUsername(
      const ::user_data_auth::GetSanitizedUsernameRequest& request) = 0;

 protected:
  // Initialize/Shutdown should be used instead.
  CryptohomeMiscClient();
  virtual ~CryptohomeMiscClient();
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source code migration is finished.
namespace ash {
using ::chromeos::CryptohomeMiscClient;
}

#endif  // CHROMEOS_DBUS_USERDATAAUTH_CRYPTOHOME_MISC_CLIENT_H_
