// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_MOCK_CLOUD_POLICY_CLIENT_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_MOCK_CLOUD_POLICY_CLIENT_H_

#include <stdint.h>

#include <string>

#include "base/macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace policy {

ACTION_P(ScheduleStatusCallback, status) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(arg0), status));
}

class MockCloudPolicyClient : public CloudPolicyClient {
 public:
  MockCloudPolicyClient();
  explicit MockCloudPolicyClient(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  explicit MockCloudPolicyClient(DeviceManagementService* service);
  MockCloudPolicyClient(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      DeviceManagementService* service);
  ~MockCloudPolicyClient() override;

  MOCK_METHOD3(SetupRegistration,
               void(const std::string&,
                    const std::string&,
                    const std::vector<std::string>&));
  MOCK_METHOD3(Register,
               void(const RegistrationParameters&,
                    const std::string&,
                    const std::string&));
  MOCK_METHOD0(FetchPolicy, void(void));
  MOCK_METHOD0(Unregister, void(void));
  MOCK_METHOD2(UploadEnterpriseMachineCertificate,
               void(const std::string&, StatusCallback));
  MOCK_METHOD2(UploadEnterpriseEnrollmentCertificate,
               void(const std::string&, StatusCallback));
  MOCK_METHOD2(UploadEnterpriseEnrollmentId,
               void(const std::string&, StatusCallback));
  void UploadDeviceStatus(
      const enterprise_management::DeviceStatusReportRequest* device_status,
      const enterprise_management::SessionStatusReportRequest* session_status,
      const enterprise_management::ChildStatusReportRequest* child_status,
      StatusCallback callback) override {
    UploadDeviceStatus_(device_status, session_status, child_status, callback);
  }

  MOCK_METHOD4(UploadDeviceStatus_,
               void(const enterprise_management::DeviceStatusReportRequest*,
                    const enterprise_management::SessionStatusReportRequest*,
                    const enterprise_management::ChildStatusReportRequest*,
                    StatusCallback&));
  MOCK_METHOD0(CancelAppInstallReportUpload, void(void));
  void UpdateGcmId(const std::string& id, StatusCallback callback) override {
    UpdateGcmId_(id, callback);
  }
  MOCK_METHOD2(UpdateGcmId_, void(const std::string&, StatusCallback&));
  MOCK_METHOD4(UploadPolicyValidationReport,
               void(CloudPolicyValidatorBase::Status,
                    const std::vector<ValueValidationIssue>&,
                    const std::string&,
                    const std::string&));

  void UploadChromeDesktopReport(
      std::unique_ptr<enterprise_management::ChromeDesktopReportRequest>
          request,
      StatusCallback callback) override {
    UploadChromeDesktopReportProxy(request.get(), callback);
  }
  // Use Proxy function because unique_ptr can't be used in mock function.
  MOCK_METHOD2(UploadChromeDesktopReportProxy,
               void(enterprise_management::ChromeDesktopReportRequest*,
                    StatusCallback&));

  void UploadChromeOsUserReport(
      std::unique_ptr<enterprise_management::ChromeOsUserReportRequest> request,
      StatusCallback callback) override {
    UploadChromeOsUserReportProxy(request.get(), callback);
  }
  // Use Proxy function because unique_ptr can't be used in mock function.
  MOCK_METHOD2(UploadChromeOsUserReportProxy,
               void(enterprise_management::ChromeOsUserReportRequest*,
                    StatusCallback&));

  void UploadRealtimeReport(base::Value value,
                            StatusCallback callback) override {
    UploadRealtimeReport_(value, callback);
  }
  MOCK_METHOD2(UploadRealtimeReport_, void(base::Value&, StatusCallback&));

  MOCK_METHOD5(ClientCertProvisioningStartCsr,
               void(const std::string& cert_scope,
                    const std::string& cert_profile_id,
                    const std::string& cert_profile_version,
                    const std::string& public_key,
                    ClientCertProvisioningStartCsrCallback callback));

  MOCK_METHOD7(ClientCertProvisioningFinishCsr,
               void(const std::string& cert_scope,
                    const std::string& cert_profile_id,
                    const std::string& cert_profile_version,
                    const std::string& public_key,
                    const std::string& va_challenge_response,
                    const std::string& signature,
                    ClientCertProvisioningFinishCsrCallback callback));

  MOCK_METHOD5(ClientCertProvisioningDownloadCert,
               void(const std::string& cert_scope,
                    const std::string& cert_profile_id,
                    const std::string& cert_profile_version,
                    const std::string& public_key,
                    ClientCertProvisioningDownloadCertCallback callback));

  // Sets the DMToken.
  void SetDMToken(const std::string& token);

  // Injects policy.
  void SetPolicy(const std::string& policy_type,
                 const std::string& settings_entity_id,
                 const enterprise_management::PolicyFetchResponse& policy);

  // Inject invalidation version.
  void SetFetchedInvalidationVersion(
      int64_t fetched_invalidation_version);

  // Sets the status field.
  void SetStatus(DeviceManagementStatus status);

  // Make the notification helpers public.
  using CloudPolicyClient::NotifyPolicyFetched;
  using CloudPolicyClient::NotifyRegistrationStateChanged;
  using CloudPolicyClient::NotifyClientError;

  using CloudPolicyClient::dm_token_;
  using CloudPolicyClient::client_id_;
  using CloudPolicyClient::last_policy_timestamp_;
  using CloudPolicyClient::public_key_version_;
  using CloudPolicyClient::public_key_version_valid_;
  using CloudPolicyClient::types_to_fetch_;
  using CloudPolicyClient::invalidation_version_;
  using CloudPolicyClient::invalidation_payload_;
  using CloudPolicyClient::fetched_invalidation_version_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCloudPolicyClient);
};

class MockCloudPolicyClientObserver : public CloudPolicyClient::Observer {
 public:
  MockCloudPolicyClientObserver();
  ~MockCloudPolicyClientObserver() override;

  MOCK_METHOD1(OnPolicyFetched, void(CloudPolicyClient*));
  MOCK_METHOD1(OnRegistrationStateChanged, void(CloudPolicyClient*));
  MOCK_METHOD1(OnClientError, void(CloudPolicyClient*));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCloudPolicyClientObserver);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_MOCK_CLOUD_POLICY_CLIENT_H_
