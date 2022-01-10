// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_CLOUD_POLICY_MANAGER_ASH_H_
#define CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_CLOUD_POLICY_MANAGER_ASH_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_scheduler.h"
#include "chrome/browser/ash/policy/server_backed_state/server_backed_state_keys_broker.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"

namespace reporting {
class MetricReportingManager;
class UserAddedRemovedReporter;
}  // namespace reporting

namespace ash {
namespace attestation {
class AttestationPolicyObserver;
class EnrollmentCertificateUploader;
class EnrollmentIdUploadManager;
class MachineCertificateUploader;
}  // namespace attestation
namespace reporting {
class LoginLogoutReporter;
}
}  // namespace ash

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace chromeos {
class InstallAttributes;
}

class PrefRegistrySimple;
class PrefService;

namespace policy {

class DeviceCloudPolicyStoreAsh;
class EuiccStatusUploader;
class ForwardingSchemaRegistry;
class HeartbeatScheduler;
class ManagedSessionService;
class SchemaRegistry;
class StatusUploader;
class SystemLogUploader;
class LookupKeyUploader;

enum class ZeroTouchEnrollmentMode { DISABLED, ENABLED, FORCED, HANDS_OFF };

// CloudPolicyManager specialization for device policy in Ash.
class DeviceCloudPolicyManagerAsh : public CloudPolicyManager {
 public:
  class Observer {
   public:
    // Invoked when the device cloud policy manager connects.
    virtual void OnDeviceCloudPolicyManagerConnected() = 0;
    // Invoked when the device cloud policy manager disconnects.
    virtual void OnDeviceCloudPolicyManagerDisconnected() = 0;
  };

  using UnregisterCallback = base::OnceCallback<void(bool)>;

  // |task_runner| is the runner for policy refresh, heartbeat, and status
  // upload tasks.
  DeviceCloudPolicyManagerAsh(
      std::unique_ptr<DeviceCloudPolicyStoreAsh> store,
      std::unique_ptr<CloudExternalDataManager> external_data_manager,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      ServerBackedStateKeysBroker* state_keys_broker);

  DeviceCloudPolicyManagerAsh(const DeviceCloudPolicyManagerAsh&) = delete;
  DeviceCloudPolicyManagerAsh& operator=(const DeviceCloudPolicyManagerAsh&) =
      delete;

  ~DeviceCloudPolicyManagerAsh() override;

  // Initializes state keys.
  void Initialize(PrefService* local_state);

  void AddDeviceCloudPolicyManagerObserver(Observer* observer);
  void RemoveDeviceCloudPolicyManagerObserver(Observer* observer);

  // CloudPolicyManager:
  void Shutdown() override;

  // Pref registration helper.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Returns the mode for using zero-touch enrollment.
  static ZeroTouchEnrollmentMode GetZeroTouchEnrollmentMode();

  // Returns the robot 'email address' associated with the device robot
  // account (sometimes called a service account) associated with this device
  // during enterprise enrollment.
  std::string GetRobotAccountId();

  // Starts the connection via |client_to_connect|.
  void StartConnection(std::unique_ptr<CloudPolicyClient> client_to_connect,
                       chromeos::InstallAttributes* install_attributes);

  // Sends the unregister request. |callback| is invoked with a boolean
  // parameter indicating the result when done.
  virtual void Unregister(UnregisterCallback callback);

  // Disconnects the manager.
  virtual void Disconnect();

  bool IsConnected() const { return core()->service() != nullptr; }

  DeviceCloudPolicyStoreAsh* device_store() { return device_store_.get(); }

  // Return the StatusUploader used to communicate device status to the
  // policy server.
  StatusUploader* GetStatusUploader() const { return status_uploader_.get(); }

  // Return the SystemLogUploader used to upload device logs to the policy
  // server.
  SystemLogUploader* GetSystemLogUploader() const {
    return syslog_uploader_.get();
  }

  // Passes the pointer to the schema registry that corresponds to the signin
  // profile.
  //
  // After this method is called, the component cloud policy manager becomes
  // associated with this schema registry.
  void SetSigninProfileSchemaRegistry(SchemaRegistry* schema_registry);

  // Sets whether the component cloud policy should be disabled (by skipping
  // the component cloud policy service creation).
  void set_component_policy_disabled_for_testing(
      bool component_policy_disabled_for_testing) {
    component_policy_disabled_for_testing_ =
        component_policy_disabled_for_testing;
  }

  // Return a pointer to the machine certificate uploader. The callers do
  // not take ownership of that pointer.
  ash::attestation::MachineCertificateUploader*
  GetMachineCertificateUploader() {
    return machine_certificate_uploader_.get();
  }

 private:
  // Saves the state keys received from |session_manager_client_|.
  void OnStateKeysUpdated();

  void NotifyConnected();
  void NotifyDisconnected();

  // Factory function to create the StatusUploader.
  void CreateStatusUploader();

  // Points to the same object as the base CloudPolicyManager::store(), but with
  // actual device policy specific type.
  std::unique_ptr<DeviceCloudPolicyStoreAsh> device_store_;

  // Manages external data referenced by device policies.
  std::unique_ptr<CloudExternalDataManager> external_data_manager_;

  ServerBackedStateKeysBroker* state_keys_broker_;

  // Helper object that handles updating the server with our current device
  // state.
  std::unique_ptr<StatusUploader> status_uploader_;

  // Helper object that handles uploading system logs to the server.
  std::unique_ptr<SystemLogUploader> syslog_uploader_;

  // Helper object that handles sending heartbeats over the GCM channel to
  // the server, to monitor connectivity.
  std::unique_ptr<HeartbeatScheduler> heartbeat_scheduler_;

  // Object that monitors managed session related events used by reporting
  // services.
  std::unique_ptr<policy::ManagedSessionService> managed_session_service_;

  // Object that reports login/logout events to the server.
  std::unique_ptr<ash::reporting::LoginLogoutReporter> login_logout_reporter_;

  // Object that reports user added/removed events to the server.
  std::unique_ptr<reporting::UserAddedRemovedReporter>
      user_added_removed_reporter_;

  // Object that initiates device metrics collection and reporting.
  std::unique_ptr<reporting::MetricReportingManager> metric_reporting_manager_;

  // The TaskRunner used to do device status and log uploads.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::CallbackListSubscription state_keys_update_subscription_;

  // PrefService instance to read the policy refresh rate from.
  PrefService* local_state_;

  std::unique_ptr<ash::attestation::EnrollmentCertificateUploader>
      enrollment_certificate_uploader_;
  std::unique_ptr<ash::attestation::EnrollmentIdUploadManager>
      enrollment_id_upload_manager_;
  std::unique_ptr<ash::attestation::MachineCertificateUploader>
      machine_certificate_uploader_;
  std::unique_ptr<ash::attestation::AttestationPolicyObserver>
      attestation_policy_observer_;
  std::unique_ptr<EuiccStatusUploader> euicc_status_uploader_;

  // Uploader for remote server unlock related lookup keys.
  std::unique_ptr<LookupKeyUploader> lookup_key_uploader_;

  // Wrapper schema registry that will track the signin profile schema registry
  // once it is passed to this class.
  std::unique_ptr<ForwardingSchemaRegistry>
      signin_profile_forwarding_schema_registry_;

  // Whether the component cloud policy should be disabled (by skipping the
  // component cloud policy service creation).
  bool component_policy_disabled_for_testing_ = false;

  base::ObserverList<Observer, true>::Unchecked observers_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_CLOUD_POLICY_MANAGER_ASH_H_
