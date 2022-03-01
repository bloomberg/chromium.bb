// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_USERDATAAUTH_FAKE_INSTALL_ATTRIBUTES_CLIENT_H_
#define CHROMEOS_DBUS_USERDATAAUTH_FAKE_INSTALL_ATTRIBUTES_CLIENT_H_

#include "chromeos/dbus/userdataauth/install_attributes_client.h"

#include "base/component_export.h"
#include "chromeos/dbus/cryptohome/UserDataAuth.pb.h"

namespace chromeos {

class COMPONENT_EXPORT(USERDATAAUTH_CLIENT) FakeInstallAttributesClient
    : public InstallAttributesClient {
 public:
  FakeInstallAttributesClient();
  ~FakeInstallAttributesClient() override;

  // Not copyable or movable.
  FakeInstallAttributesClient(const FakeInstallAttributesClient&) = delete;
  FakeInstallAttributesClient& operator=(const FakeInstallAttributesClient&) =
      delete;

  // Checks that a FakeInstallAttributesClient instance was initialized and
  // returns it.
  static FakeInstallAttributesClient* Get();

  // InstallAttributesClient override:
  void WaitForServiceToBeAvailable(
      chromeos::WaitForServiceToBeAvailableCallback callback) override;
  void InstallAttributesGet(
      const ::user_data_auth::InstallAttributesGetRequest& request,
      InstallAttributesGetCallback callback) override;
  void InstallAttributesSet(
      const ::user_data_auth::InstallAttributesSetRequest& request,
      InstallAttributesSetCallback callback) override;
  void InstallAttributesFinalize(
      const ::user_data_auth::InstallAttributesFinalizeRequest& request,
      InstallAttributesFinalizeCallback callback) override;
  void InstallAttributesGetStatus(
      const ::user_data_auth::InstallAttributesGetStatusRequest& request,
      InstallAttributesGetStatusCallback callback) override;
  void RemoveFirmwareManagementParameters(
      const ::user_data_auth::RemoveFirmwareManagementParametersRequest&
          request,
      RemoveFirmwareManagementParametersCallback callback) override;
  void SetFirmwareManagementParameters(
      const ::user_data_auth::SetFirmwareManagementParametersRequest& request,
      SetFirmwareManagementParametersCallback callback) override;
  absl::optional<::user_data_auth::InstallAttributesGetReply>
  BlockingInstallAttributesGet(
      const ::user_data_auth::InstallAttributesGetRequest& request) override;
  absl::optional<::user_data_auth::InstallAttributesSetReply>
  BlockingInstallAttributesSet(
      const ::user_data_auth::InstallAttributesSetRequest& request) override;
  absl::optional<::user_data_auth::InstallAttributesFinalizeReply>
  BlockingInstallAttributesFinalize(
      const ::user_data_auth::InstallAttributesFinalizeRequest& request)
      override;
  absl::optional<::user_data_auth::InstallAttributesGetStatusReply>
  BlockingInstallAttributesGetStatus(
      const ::user_data_auth::InstallAttributesGetStatusRequest& request)
      override;

  // FWMP related:

  // Return the number of times RemoveFirmwareManagementParameters is called.
  int remove_firmware_management_parameters_from_tpm_call_count() const {
    return remove_firmware_management_parameters_from_tpm_call_count_;
  }

  // WaitForServiceToBeAvailable() related:

  // Changes the behavior of WaitForServiceToBeAvailable(). This method runs
  // pending callbacks if is_available is true.
  void SetServiceIsAvailable(bool is_available);

  // Runs pending availability callbacks reporting that the service is
  // unavailable. Expects service not to be available when called.
  void ReportServiceIsNotAvailable();

 private:
  // Helper that returns the protobuf reply.
  template <typename ReplyType>
  void ReturnProtobufMethodCallback(const ReplyType& reply,
                                    DBusMethodCallback<ReplyType> callback);

  // Loads install attributes from the stub file.
  bool LoadInstallAttributes();

  // FWMP related:

  // Number of times RemoveFirmwareManagementParameters() is called.
  int remove_firmware_management_parameters_from_tpm_call_count_ = 0;

  // Install attributes related:

  // A stub store for InstallAttributes, mapping an attribute name to the
  // associated data blob. Used to implement InstallAttributesSet and -Get.
  std::map<std::string, std::string> install_attrs_;

  // Set to true if install attributes are finalized.
  bool locked_;

  // WaitForServiceToBeAvailable() related fields:

  // If set, we tell callers that service is available.
  bool service_is_available_ = true;

  // If set, WaitForServiceToBeAvailable will run the callback, even if service
  // is not available (instead of adding the callback to pending callback list).
  bool service_reported_not_available_ = false;

  // The list of callbacks passed to WaitForServiceToBeAvailable when the
  // service wasn't available.
  std::vector<WaitForServiceToBeAvailableCallback>
      pending_wait_for_service_to_be_available_callbacks_;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::FakeInstallAttributesClient;
}

#endif  // CHROMEOS_DBUS_USERDATAAUTH_FAKE_INSTALL_ATTRIBUTES_CLIENT_H_
