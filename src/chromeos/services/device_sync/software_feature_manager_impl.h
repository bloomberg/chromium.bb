// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_SOFTWARE_FEATURE_MANAGER_IMPL_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_SOFTWARE_FEATURE_MANAGER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/components/multidevice/software_feature.h"
#include "chromeos/services/device_sync/feature_status_change.h"
#include "chromeos/services/device_sync/network_request_error.h"
#include "chromeos/services/device_sync/proto/cryptauth_api.pb.h"
#include "chromeos/services/device_sync/software_feature_manager.h"

namespace chromeos {

namespace device_sync {

class CryptAuthClient;
class CryptAuthClientFactory;
class CryptAuthFeatureStatusSetter;

// Concrete SoftwareFeatureManager implementation. To query and/or set
// MultiDevice hosts, this class makes network requests to the CryptAuth
// back-end.
class SoftwareFeatureManagerImpl : public SoftwareFeatureManager {
 public:
  class Factory {
   public:
    static std::unique_ptr<SoftwareFeatureManager> Create(
        CryptAuthClientFactory* cryptauth_client_factory,
        CryptAuthFeatureStatusSetter* feature_status_setter);

    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<SoftwareFeatureManager> CreateInstance(
        CryptAuthClientFactory* cryptauth_client_factory,
        CryptAuthFeatureStatusSetter* feature_status_setter) = 0;

   private:
    static Factory* test_factory_instance_;
  };

  ~SoftwareFeatureManagerImpl() override;

  // SoftwareFeatureManager:
  void SetSoftwareFeatureState(
      const std::string& public_key,
      multidevice::SoftwareFeature software_feature,
      bool enabled,
      const base::Closure& success_callback,
      const base::Callback<void(NetworkRequestError)>& error_callback,
      bool is_exclusive = false) override;
  void SetFeatureStatus(
      const std::string& device_id,
      multidevice::SoftwareFeature feature,
      FeatureStatusChange status_change,
      base::OnceClosure success_callback,
      base::OnceCallback<void(NetworkRequestError)> error_callback) override;
  void FindEligibleDevices(
      multidevice::SoftwareFeature software_feature,
      const base::Callback<void(
          const std::vector<cryptauth::ExternalDeviceInfo>&,
          const std::vector<cryptauth::IneligibleDevice>&)>& success_callback,
      const base::Callback<void(NetworkRequestError)>& error_callback) override;

 private:
  enum class RequestType {
    kSetSoftwareFeature,
    kSetFeatureStatus,
    kFindEligibleMultideviceHosts
  };

  struct Request {
    // Used for kSetSoftwareFeature Requests.
    Request(std::unique_ptr<cryptauth::ToggleEasyUnlockRequest> toggle_request,
            const base::Closure& set_software_success_callback,
            const base::Callback<void(NetworkRequestError)> error_callback);

    // Used for kSetFeatureStatus Requests.
    Request(const std::string& device_id,
            multidevice::SoftwareFeature feature,
            FeatureStatusChange status_change,
            base::OnceClosure set_feature_status_success_callback,
            base::OnceCallback<void(NetworkRequestError)>
                set_feature_status_error_callback);

    // Used for kFindEligibleMultideviceHosts Requests.
    Request(std::unique_ptr<cryptauth::FindEligibleUnlockDevicesRequest>
                find_request,
            const base::Callback<
                void(const std::vector<cryptauth::ExternalDeviceInfo>&,
                     const std::vector<cryptauth::IneligibleDevice>&)>
                find_hosts_success_callback,
            const base::Callback<void(NetworkRequestError)> error_callback);

    ~Request();

    RequestType request_type;

    // Set for kSetSoftwareFeature and kFindEligibleMultideviceHosts; unset
    // otherwise.
    const base::Callback<void(NetworkRequestError)> error_callback;

    // Set for kSetSoftwareFeature; unset otherwise.
    std::unique_ptr<cryptauth::ToggleEasyUnlockRequest> toggle_request;
    const base::Closure set_software_success_callback;

    // Set for kSetFeatureStatus; unset otherwise.
    std::string device_id;
    multidevice::SoftwareFeature feature;
    FeatureStatusChange status_change;
    base::OnceClosure set_feature_status_success_callback;
    base::OnceCallback<void(NetworkRequestError)>
        set_feature_status_error_callback;

    // Set for kFindEligibleMultideviceHosts; unset otherwise.
    std::unique_ptr<cryptauth::FindEligibleUnlockDevicesRequest> find_request;
    const base::Callback<void(const std::vector<cryptauth::ExternalDeviceInfo>&,
                              const std::vector<cryptauth::IneligibleDevice>&)>
        find_hosts_success_callback;
  };

  SoftwareFeatureManagerImpl(
      CryptAuthClientFactory* cryptauth_client_factory,
      CryptAuthFeatureStatusSetter* feature_status_setter);

  void ProcessRequestQueue();
  void ProcessSetSoftwareFeatureStateRequest();
  void ProcessSetFeatureStatusRequest();
  void ProcessFindEligibleDevicesRequest();

  void OnToggleEasyUnlockResponse(
      const cryptauth::ToggleEasyUnlockResponse& response);
  void OnSetFeatureStatusSuccess();
  void OnFindEligibleUnlockDevicesResponse(
      const cryptauth::FindEligibleUnlockDevicesResponse& response);
  void OnErrorResponse(NetworkRequestError response);
  void OnSetFeatureStatusError(NetworkRequestError response);

  CryptAuthClientFactory* crypt_auth_client_factory_;

  // Non-null only when v2 DeviceSync is enabled.
  CryptAuthFeatureStatusSetter* feature_status_setter_ = nullptr;

  std::unique_ptr<CryptAuthClient> current_cryptauth_client_;
  std::unique_ptr<Request> current_request_;
  base::queue<std::unique_ptr<Request>> pending_requests_;

  base::WeakPtrFactory<SoftwareFeatureManagerImpl> weak_ptr_factory_{this};
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_SOFTWARE_FEATURE_MANAGER_IMPL_H_
