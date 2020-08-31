// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_DEVICE_SYNC_IMPL_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_DEVICE_SYNC_IMPL_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "chromeos/services/device_sync/cryptauth_device_activity_getter.h"
#include "chromeos/services/device_sync/cryptauth_enrollment_manager.h"
#include "chromeos/services/device_sync/cryptauth_gcm_manager.h"
#include "chromeos/services/device_sync/device_sync_base.h"
#include "chromeos/services/device_sync/feature_status_change.h"
#include "chromeos/services/device_sync/network_request_error.h"
#include "chromeos/services/device_sync/proto/cryptauth_common.pb.h"
#include "chromeos/services/device_sync/public/mojom/device_sync.mojom.h"
#include "chromeos/services/device_sync/remote_device_provider.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class Clock;
class OneShotTimer;
}  // namespace base

namespace gcm {
class GCMDriver;
}  // namespace gcm

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace chromeos {

namespace device_sync {

class ClientAppMetadataProvider;
class CryptAuthClientFactory;
class CryptAuthDeviceManager;
class CryptAuthDeviceNotifier;
class CryptAuthDeviceRegistry;
class CryptAuthFeatureStatusSetter;
class CryptAuthKeyRegistry;
class CryptAuthScheduler;
class CryptAuthV2DeviceManager;
class GcmDeviceInfoProvider;
class SoftwareFeatureManager;

// Concrete DeviceSync implementation. When DeviceSyncImpl is constructed, it
// starts an initialization flow with the following steps:
// (1) Check if the primary user is logged in with a valid account ID.
// (2) If not, wait for IdentityManager to provide the primary account ID.
// (3) Instantiate classes which communicate with the CryptAuth back-end.
// (4) Check enrollment state; if not yet enrolled, enroll the device.
// (5) When enrollment is valid, listen for device sync updates.
class DeviceSyncImpl : public DeviceSyncBase,
                       public signin::IdentityManager::Observer,
                       public CryptAuthEnrollmentManager::Observer,
                       public RemoteDeviceProvider::Observer {
 public:
  class Factory {
   public:
    // Note: |timer| should be a newly-created base::OneShotTimer object; this
    // parameter only exists for testing via dependency injection.
    static std::unique_ptr<DeviceSyncBase> Create(
        signin::IdentityManager* identity_manager,
        gcm::GCMDriver* gcm_driver,
        PrefService* profile_prefs,
        const GcmDeviceInfoProvider* gcm_device_info_provider,
        ClientAppMetadataProvider* client_app_metadata_provider,
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
        std::unique_ptr<base::OneShotTimer> timer);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<DeviceSyncBase> CreateInstance(
        signin::IdentityManager* identity_manager,
        gcm::GCMDriver* gcm_driver,
        PrefService* profile_prefs,
        const GcmDeviceInfoProvider* gcm_device_info_provider,
        ClientAppMetadataProvider* client_app_metadata_provider,
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
        std::unique_ptr<base::OneShotTimer> timer) = 0;

   private:
    static Factory* test_factory_instance_;
  };

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  ~DeviceSyncImpl() override;

 protected:
  // mojom::DeviceSync:
  void ForceEnrollmentNow(ForceEnrollmentNowCallback callback) override;
  void ForceSyncNow(ForceSyncNowCallback callback) override;
  void GetLocalDeviceMetadata(GetLocalDeviceMetadataCallback callback) override;
  void GetSyncedDevices(GetSyncedDevicesCallback callback) override;
  void SetSoftwareFeatureState(
      const std::string& device_public_key,
      multidevice::SoftwareFeature software_feature,
      bool enabled,
      bool is_exclusive,
      SetSoftwareFeatureStateCallback callback) override;
  void SetFeatureStatus(const std::string& device_instance_id,
                        multidevice::SoftwareFeature feature,
                        FeatureStatusChange status_change,
                        SetFeatureStatusCallback callback) override;
  void FindEligibleDevices(multidevice::SoftwareFeature software_feature,
                           FindEligibleDevicesCallback callback) override;
  void NotifyDevices(const std::vector<std::string>& device_instance_ids,
                     cryptauthv2::TargetService target_service,
                     multidevice::SoftwareFeature feature,
                     NotifyDevicesCallback callback) override;
  void GetDevicesActivityStatus(
      GetDevicesActivityStatusCallback callback) override;
  void GetDebugInfo(GetDebugInfoCallback callback) override;

  // CryptAuthEnrollmentManager::Observer:
  void OnEnrollmentFinished(bool success) override;

  // RemoteDeviceProvider::Observer:
  void OnSyncDeviceListChanged() override;

 private:
  friend class DeviceSyncServiceTest;

  enum class Status { FETCHING_ACCOUNT_INFO, WAITING_FOR_ENROLLMENT, READY };

  class PendingSetSoftwareFeatureRequest {
   public:
    PendingSetSoftwareFeatureRequest(
        const std::string& device_public_key,
        multidevice::SoftwareFeature software_feature,
        bool enabled,
        RemoteDeviceProvider* remote_device_provider,
        SetSoftwareFeatureStateCallback callback);
    ~PendingSetSoftwareFeatureRequest();

    // Whether the request has been fulfilled (i.e., whether the requested
    // feature has been set according to the parameters used).
    bool IsFulfilled() const;

    void InvokeCallback(mojom::NetworkRequestResult result);

    multidevice::SoftwareFeature software_feature() const {
      return software_feature_;
    }

    bool enabled() const { return enabled_; }

   private:
    std::string device_public_key_;
    multidevice::SoftwareFeature software_feature_;
    bool enabled_;
    RemoteDeviceProvider* remote_device_provider_;
    SetSoftwareFeatureStateCallback callback_;
  };

  class PendingSetFeatureStatusRequest {
   public:
    PendingSetFeatureStatusRequest(
        const std::string& device_instance_id,
        multidevice::SoftwareFeature software_feature,
        FeatureStatusChange status_change,
        RemoteDeviceProvider* remote_device_provider,
        SetFeatureStatusCallback callback);
    ~PendingSetFeatureStatusRequest();

    // True if the device and software feature status specified in the request
    // agrees with the device data returned by CryptAuth.
    bool IsFulfilled() const;

    void InvokeCallback(mojom::NetworkRequestResult result);

   private:
    std::string device_instance_id_;
    multidevice::SoftwareFeature software_feature_;
    FeatureStatusChange status_change_;
    RemoteDeviceProvider* remote_device_provider_;
    SetFeatureStatusCallback callback_;
  };

  DeviceSyncImpl(
      signin::IdentityManager* identity_manager,
      gcm::GCMDriver* gcm_driver,
      PrefService* profile_prefs,
      const GcmDeviceInfoProvider* gcm_device_info_provider,
      ClientAppMetadataProvider* client_app_metadata_provider,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      base::Clock* clock,
      std::unique_ptr<base::OneShotTimer> timer);

  // DeviceSyncBase:
  void Shutdown() override;

  // signin::IdentityManager::Observer:
  void OnUnconsentedPrimaryAccountChanged(
      const CoreAccountInfo& primary_account_info) override;

  void ProcessPrimaryAccountInfo(const CoreAccountInfo& primary_account_info);
  void InitializeCryptAuthManagementObjects();
  void CompleteInitializationAfterSuccessfulEnrollment();

  base::Optional<multidevice::RemoteDevice> GetSyncedDeviceWithPublicKey(
      const std::string& public_key) const;

  void OnSetSoftwareFeatureStateSuccess();
  void OnSetSoftwareFeatureStateError(const base::UnguessableToken& request_id,
                                      NetworkRequestError error);
  void OnSetFeatureStatusSuccess();
  void OnSetFeatureStatusError(const base::UnguessableToken& request_id,
                               NetworkRequestError error);
  void OnFindEligibleDevicesSuccess(
      const base::RepeatingCallback<
          void(mojom::NetworkRequestResult,
               mojom::FindEligibleDevicesResponsePtr)>& callback,
      const std::vector<cryptauth::ExternalDeviceInfo>& eligible_devices,
      const std::vector<cryptauth::IneligibleDevice>& ineligible_devices);
  void OnFindEligibleDevicesError(
      const base::RepeatingCallback<
          void(mojom::NetworkRequestResult,
               mojom::FindEligibleDevicesResponsePtr)>& callback,
      NetworkRequestError error);
  void OnNotifyDevicesSuccess(const base::UnguessableToken& request_id);
  void OnNotifyDevicesError(const base::UnguessableToken& request_id,
                            NetworkRequestError error);
  void OnGetDevicesActivityStatusFinished(
      const base::UnguessableToken& request_id,
      CryptAuthDeviceActivityGetter::DeviceActivityStatusResult
          device_activity_status_result);
  void OnGetDevicesActivityStatusError(
      const base::UnguessableToken& request_id,
      NetworkRequestError network_request_error);

  // Note: If the timer is already running, StartSetSoftwareFeatureTimer()
  // restarts it.
  void StartSetSoftwareFeatureTimer();
  void OnSetSoftwareFeatureTimerFired();

  signin::IdentityManager* identity_manager_;
  gcm::GCMDriver* gcm_driver_;
  PrefService* profile_prefs_;
  const GcmDeviceInfoProvider* gcm_device_info_provider_;
  ClientAppMetadataProvider* client_app_metadata_provider_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  base::Clock* clock_;
  std::unique_ptr<base::OneShotTimer> set_software_feature_timer_;

  Status status_;
  CoreAccountInfo primary_account_info_;
  base::flat_map<base::UnguessableToken,
                 std::unique_ptr<PendingSetSoftwareFeatureRequest>>
      id_to_pending_set_software_feature_request_map_;
  base::flat_map<base::UnguessableToken,
                 std::unique_ptr<PendingSetFeatureStatusRequest>>
      id_to_pending_set_feature_status_request_map_;
  base::flat_map<base::UnguessableToken, NotifyDevicesCallback>
      pending_notify_devices_callbacks_;
  base::flat_map<base::UnguessableToken, GetDevicesActivityStatusCallback>
      get_devices_activity_status_callbacks_;

  std::unique_ptr<CryptAuthGCMManager> cryptauth_gcm_manager_;
  std::unique_ptr<CryptAuthClientFactory> cryptauth_client_factory_;

  // Only created and used if v2 Enrollment is enabled; null otherwise.
  std::unique_ptr<CryptAuthKeyRegistry> cryptauth_key_registry_;
  std::unique_ptr<CryptAuthScheduler> cryptauth_scheduler_;

  // Only created and used if v2 DeviceSync is enabled; null otherwise.
  std::unique_ptr<CryptAuthDeviceRegistry> cryptauth_device_registry_;
  std::unique_ptr<CryptAuthV2DeviceManager> cryptauth_v2_device_manager_;
  std::unique_ptr<CryptAuthDeviceNotifier> device_notifier_;
  std::unique_ptr<CryptAuthFeatureStatusSetter> feature_status_setter_;

  std::unique_ptr<CryptAuthEnrollmentManager> cryptauth_enrollment_manager_;
  std::unique_ptr<CryptAuthDeviceManager> cryptauth_device_manager_;
  std::unique_ptr<RemoteDeviceProvider> remote_device_provider_;
  std::unique_ptr<SoftwareFeatureManager> software_feature_manager_;
  std::unique_ptr<CryptAuthDeviceActivityGetter>
      cryptauth_device_activity_getter_;

  base::WeakPtrFactory<DeviceSyncImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DeviceSyncImpl);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_DEVICE_SYNC_IMPL_H_
