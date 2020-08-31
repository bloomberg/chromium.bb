// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SESSION_MANAGER_SESSION_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_SESSION_MANAGER_SESSION_MANAGER_CLIENT_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "third_party/cros_system_api/dbus/login_manager/dbus-constants.h"

namespace cryptohome {
class AccountIdentifier;
}

namespace dbus {
class Bus;
}

namespace enterprise_management {
class SignedData;
}

namespace login_manager {
class LoginScreenStorageMetadata;
class PolicyDescriptor;
class StartArcMiniContainerRequest;
class UpgradeArcContainerRequest;
}  // namespace login_manager

namespace chromeos {

// SessionManagerClient is used to communicate with the session manager.
class COMPONENT_EXPORT(SESSION_MANAGER) SessionManagerClient {
 public:
  // The result type received from session manager on request to retrieve the
  // policy. Used to define the buckets for an enumerated UMA histogram.
  // Hence,
  //   (a) existing enumerated constants should never be deleted or reordered.
  //   (b) new constants should be inserted immediately before COUNT.
  enum class RetrievePolicyResponseType {
    // Other type of error while retrieving policy data (e.g. D-Bus timeout).
    OTHER_ERROR = 0,
    // The policy was retrieved successfully.
    SUCCESS = 1,
    // Retrieve policy request issued before session started (deprecated, use
    // GET_SERVICE_FAIL).
    SESSION_DOES_NOT_EXIST_DEPRECATED = 2,
    // Session manager failed to encode the policy data.
    POLICY_ENCODE_ERROR = 3,
    // Session manager failed to get the policy service, possibly because a user
    // session hasn't started yet or the account id was invalid.
    GET_SERVICE_FAIL = 4,
    // Has to be the last value of enumeration. Used for UMA.
    COUNT
  };

  enum class AdbSideloadResponseCode {
    // ADB sideload operation has finished successfully.
    SUCCESS = 1,
    // ADB sideload operation has failed.
    FAILED = 2,
    // ADB sideload requires a powerwash to unblock (to define nvram).
    NEED_POWERWASH = 3,
  };

  // Interface for observing changes from the session manager.
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when the owner key is set.
    virtual void OwnerKeySet(bool success) {}

    // Called when the property change is complete.
    virtual void PropertyChangeComplete(bool success) {}

    // Called after EmitLoginPromptVisible is called.
    virtual void EmitLoginPromptVisibleCalled() {}

    // Called when the ARC instance is stopped after it had already started.
    virtual void ArcInstanceStopped() {}
  };

  // Interface for performing actions on behalf of the stub implementation.
  class StubDelegate {
   public:
    virtual ~StubDelegate() {}

    // Locks the screen. Invoked by the stub when RequestLockScreen() is called.
    // In the real implementation of SessionManagerClient::RequestLockScreen(),
    // a lock request is forwarded to the session manager; in the stub, this is
    // short-circuited and the screen is locked immediately.
    virtual void LockScreenForStub() = 0;
  };

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Creates and initializes an InMemory fake global instance for testing.
  static void InitializeFakeInMemory();

  // Destroys the global instance which must have been initialized.
  static void Shutdown();

  // Returns the global instance if initialized. May return null.
  static SessionManagerClient* Get();

  // Sets the delegate used by the stub implementation. Ownership of |delegate|
  // remains with the caller.
  virtual void SetStubDelegate(StubDelegate* delegate) = 0;

  // Adds or removes an observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
  virtual bool HasObserver(const Observer* observer) const = 0;

  // Runs the callback as soon as the service becomes available.
  virtual void WaitForServiceToBeAvailable(
      WaitForServiceToBeAvailableCallback callback) = 0;

  // Returns the most recent screen-lock state received from session_manager.
  // This method should only be called by low-level code that is unable to
  // depend on UI code and get the lock state from it instead.
  virtual bool IsScreenLocked() const = 0;

  // Kicks off an attempt to emit the "login-prompt-visible" upstart signal.
  virtual void EmitLoginPromptVisible() = 0;

  // Kicks off an attempt to emit the "ash-initialized" upstart signal.
  virtual void EmitAshInitialized() = 0;

  // Restarts the browser job, passing |argv| as the updated command line.
  // The session manager requires a RestartJob caller to open a socket pair and
  // pass one end while holding the local end open for the duration of the call.
  // The session manager uses this to determine whether the PID the restart
  // request originates from belongs to the browser itself.
  // This method duplicates |socket_fd| so it's OK to close the FD without
  // waiting for the result.
  virtual void RestartJob(int socket_fd,
                          const std::vector<std::string>& argv,
                          VoidDBusMethodCallback callback) = 0;

  // Sends the user's password to the session manager.
  virtual void SaveLoginPassword(const std::string& password) = 0;

  // Used to report errors from |LoginScreenStorageStore()|. |error| should
  // contain an error message if an error occurred.
  using LoginScreenStorageStoreCallback =
      DBusMethodCallback<std::string /* error */>;

  // Stores data to the login screen storage. login screen storage is a D-Bus
  // API that is used by the custom login screen implementations to inject
  // credentials into the session and store persistent data across the login
  // screen restarts.
  virtual void LoginScreenStorageStore(
      const std::string& key,
      const login_manager::LoginScreenStorageMetadata& metadata,
      const std::string& data,
      LoginScreenStorageStoreCallback callback) = 0;

  // Used for |LoginScreenStorageRetrieve()| method. |data| argument is the data
  // returned by the session manager. |error| contains an error message if an
  // error occurred, otherwise empty.
  using LoginScreenStorageRetrieveCallback =
      base::OnceCallback<void(base::Optional<std::string> /* data */,
                              base::Optional<std::string> /* error */)>;

  // Retrieve data stored earlier with the |LoginScreenStorageStore()| method.
  virtual void LoginScreenStorageRetrieve(
      const std::string& key,
      LoginScreenStorageRetrieveCallback callback) = 0;

  // Used for |LoginScreenStorageListKeys()| method. |keys| argument is the list
  // of keys currently stored in the login screen storage. In case of error,
  // |keys| is empty and |error| contains the error message.
  using LoginScreenStorageListKeysCallback =
      base::OnceCallback<void(std::vector<std::string> /* keys */,
                              base::Optional<std::string> /* error */)>;

  // List all keys currently stored in the login screen storage.
  virtual void LoginScreenStorageListKeys(
      LoginScreenStorageListKeysCallback callback) = 0;

  // Delete a key and the value associated with it from the login screen
  // storage.
  virtual void LoginScreenStorageDelete(const std::string& key) = 0;

  // Starts the session for the user.
  virtual void StartSession(
      const cryptohome::AccountIdentifier& cryptohome_id) = 0;

  // Stops the current session. Don't call directly unless there's no user on
  // the device. Use SessionTerminationManager::StopSession instead.
  virtual void StopSession(login_manager::SessionStopReason reason) = 0;

  // Starts the factory reset.
  virtual void StartDeviceWipe() = 0;

  // Starts a remotely initiated factory reset, similar to |StartDeviceWipe|
  // above, but also performs additional checks on Chrome OS side.
  virtual void StartRemoteDeviceWipe(
      const enterprise_management::SignedData& signed_command) = 0;

  // Set the block_demode and check_enrollment flags to 0 in the VPD.
  virtual void ClearForcedReEnrollmentVpd(VoidDBusMethodCallback callback) = 0;

  // Triggers a TPM firmware update.
  virtual void StartTPMFirmwareUpdate(const std::string& update_mode) = 0;

  // Sends a request to lock the screen to session_manager. Locking occurs
  // asynchronously.
  virtual void RequestLockScreen() = 0;

  // Notifies session_manager that Chrome has shown the lock screen.
  virtual void NotifyLockScreenShown() = 0;

  // Notifies session_manager that Chrome has hidden the lock screen.
  virtual void NotifyLockScreenDismissed() = 0;

  // Map that is used to describe the set of active user sessions where |key|
  // is cryptohome id and |value| is user_id_hash.
  using ActiveSessionsMap = std::map<std::string, std::string>;

  // The ActiveSessionsCallback is used for the RetrieveActiveSessions()
  // method. It receives |sessions| argument where the keys are cryptohome_ids
  // for all users that are currently active.
  using ActiveSessionsCallback =
      DBusMethodCallback<ActiveSessionsMap /* sessions */>;

  // Enumerates active user sessions. Usually Chrome naturally keeps track of
  // active users when they are added into current session. When Chrome is
  // restarted after crash by session_manager it only receives cryptohome id and
  // user_id_hash for one user. This method is used to retrieve list of all
  // active users.
  virtual void RetrieveActiveSessions(ActiveSessionsCallback callback) = 0;

  // TODO(crbug.com/765644): Change the policy storage interface so that it has
  // a single StorePolicy, RetrievePolicy, BlockingRetrivePolicy method that
  // takes a PolicyDescriptor.

  // Used for RetrieveDevicePolicy, RetrievePolicyForUser and
  // RetrieveDeviceLocalAccountPolicy. Takes a serialized protocol buffer as
  // string.  Upon success, we will pass a protobuf and SUCCESS |response_type|
  // to the callback. On failure, we will pass "" and the details of error type
  // in |response_type|.
  using RetrievePolicyCallback =
      base::OnceCallback<void(RetrievePolicyResponseType response_type,
                              const std::string& protobuf)>;

  // Fetches the device policy blob stored by the session manager.  Upon
  // completion of the retrieve attempt, we will call the provided callback.
  // DEPRECATED, use RetrievePolicy() instead.
  virtual void RetrieveDevicePolicy(RetrievePolicyCallback callback) = 0;

  // Same as RetrieveDevicePolicy() but blocks until a reply is received, and
  // populates the policy synchronously. Returns SUCCESS when successful, or
  // the corresponding error from enum in case of a failure.
  // This may only be called in situations where blocking the UI thread is
  // considered acceptable (e.g. restarting the browser after a crash or after
  // a flag change).
  // TODO(crbug.com/160522): Get rid of blocking calls.
  // DEPRECATED, use BlockingRetrievePolicy() instead.
  virtual RetrievePolicyResponseType BlockingRetrieveDevicePolicy(
      std::string* policy_out) = 0;

  // Fetches the user policy blob stored by the session manager for the given
  // |cryptohome_id|. Upon completion of the retrieve attempt, we will call the
  // provided callback.
  // DEPRECATED, use RetrievePolicy() instead.
  virtual void RetrievePolicyForUser(
      const cryptohome::AccountIdentifier& cryptohome_id,
      RetrievePolicyCallback callback) = 0;

  // Same as RetrievePolicyForUser() but blocks until a reply is received, and
  // populates the policy synchronously. Returns SUCCESS when successful, or
  // the corresponding error from enum in case of a failure.
  // This may only be called in situations where blocking the UI thread is
  // considered acceptable (e.g. restarting the browser after a crash or after
  // a flag change).
  // TODO(crbug.com/160522): Get rid of blocking calls.
  // DEPRECATED, use BlockingRetrievePolicy() instead.
  virtual RetrievePolicyResponseType BlockingRetrievePolicyForUser(
      const cryptohome::AccountIdentifier& cryptohome_id,
      std::string* policy_out) = 0;

  // Fetches the user policy blob for a hidden user home mount. |callback| is
  // invoked upon completition.
  // DEPRECATED, use RetrievePolicy() instead.
  virtual void RetrievePolicyForUserWithoutSession(
      const cryptohome::AccountIdentifier& cryptohome_id,
      RetrievePolicyCallback callback) = 0;

  // Fetches the policy blob associated with the specified device-local account
  // from session manager.  |callback| is invoked up on completion.
  // DEPRECATED, use RetrievePolicy() instead.
  virtual void RetrieveDeviceLocalAccountPolicy(
      const std::string& account_id,
      RetrievePolicyCallback callback) = 0;

  // Same as RetrieveDeviceLocalAccountPolicy() but blocks until a reply is
  // received, and populates the policy synchronously. Returns SUCCESS when
  // successful, or the corresponding error from enum in case of a failure.
  // This may only be called in situations where blocking the UI thread is
  // considered acceptable (e.g. restarting the browser after a crash or after
  // a flag change).
  // TODO(crbug.com/165022): Get rid of blocking calls.
  // DEPRECATED, use BlockingRetrievePolicy() instead.
  virtual RetrievePolicyResponseType BlockingRetrieveDeviceLocalAccountPolicy(
      const std::string& account_id,
      std::string* policy_out) = 0;

  // Fetches a policy blob stored by the session manager. Invokes |callback|
  // upon completion.
  virtual void RetrievePolicy(const login_manager::PolicyDescriptor& descriptor,
                              RetrievePolicyCallback callback) = 0;

  // Same as RetrievePolicy() but blocks until a reply is
  // received, and populates the policy synchronously. Returns SUCCESS when
  // successful, or the corresponding error from enum in case of a failure.
  // This may only be called in situations where blocking the UI thread is
  // considered acceptable (e.g. restarting the browser after a crash or after
  // a flag change).
  // TODO(crbug.com/165022): Get rid of blocking calls.
  virtual RetrievePolicyResponseType BlockingRetrievePolicy(
      const login_manager::PolicyDescriptor& descriptor,
      std::string* policy_out) = 0;

  // Attempts to asynchronously store |policy_blob| as device policy.  Upon
  // completion of the store attempt, we will call callback.
  virtual void StoreDevicePolicy(const std::string& policy_blob,
                                 VoidDBusMethodCallback callback) = 0;

  // Attempts to asynchronously store |policy_blob| as user policy for the
  // given |cryptohome_id|. Upon completion of the store attempt, we will call
  // callback.
  virtual void StorePolicyForUser(
      const cryptohome::AccountIdentifier& cryptohome_id,
      const std::string& policy_blob,
      VoidDBusMethodCallback callback) = 0;

  // Sends a request to store a policy blob for the specified device-local
  // account. The result of the operation is reported through |callback|.
  virtual void StoreDeviceLocalAccountPolicy(
      const std::string& account_id,
      const std::string& policy_blob,
      VoidDBusMethodCallback callback) = 0;

  // Sends a request to store a |policy_blob| to Session Manager. The storage
  // location is determined by |descriptor|. The result of the operation is
  // reported through |callback|.
  virtual void StorePolicy(const login_manager::PolicyDescriptor& descriptor,
                           const std::string& policy_blob,
                           VoidDBusMethodCallback callback) = 0;

  // Returns whether session manager can be used to restart Chrome in order to
  // apply per-user session flags, or start guest session.
  // This returns true for the real session manager client implementation, and
  // false for the fake (unless explicitly set by a test).
  virtual bool SupportsBrowserRestart() const = 0;

  // Sets the flags to be applied next time by the session manager when Chrome
  // is restarted inside an already started session for a particular user.
  virtual void SetFlagsForUser(
      const cryptohome::AccountIdentifier& cryptohome_id,
      const std::vector<std::string>& flags) = 0;

  using StateKeysCallback =
      base::OnceCallback<void(const std::vector<std::string>& state_keys)>;

  // Get the currently valid server-backed state keys for the device.
  // Server-backed state keys are opaque, device-unique, time-dependent,
  // client-determined identifiers that are used for keying state in the cloud
  // for the device to retrieve after a device factory reset.
  //
  // The state keys are returned asynchronously via |callback|. The callback
  // is invoked with an empty state key vector in case of errors. If the time
  // sync fails or there's no network, the callback is never invoked.
  virtual void GetServerBackedStateKeys(StateKeysCallback callback) = 0;

  // StartArcMiniContainer starts a container with only a handful of ARC
  // processes for Chrome OS login screen.
  virtual void StartArcMiniContainer(
      const login_manager::StartArcMiniContainerRequest& request,
      VoidDBusMethodCallback callback) = 0;

  // UpgradeArcContainer upgrades a mini-container to a full ARC container. On
  // upgrade failure, the container will be shutdown. The container shutdown
  // will trigger the ArcInstanceStopped signal (as usual). There are no
  // guarantees over whether this |callback| is invoked or the
  // ArcInstanceStopped signal is received first.
  virtual void UpgradeArcContainer(
      const login_manager::UpgradeArcContainerRequest& request,
      VoidDBusMethodCallback callback) = 0;

  // Asynchronously stops the ARC instance. When |should_backup_log| is set to
  // true it also initiates ARC log back up operation on debugd for the given
  // |account_id|. Upon completion, invokes |callback| with the result;
  // true on success, false on failure (either session manager failed to
  // stop an instance or session manager can not be reached).
  virtual void StopArcInstance(const std::string& account_id,
                               bool should_backup_log,
                               VoidDBusMethodCallback callback) = 0;

  // Adjusts the amount of CPU the ARC instance is allowed to use. When
  // |restriction_state| is CONTAINER_CPU_RESTRICTION_FOREGROUND the limit is
  // adjusted so ARC can use all the system's CPU if needed. When it is
  // CONTAINER_CPU_RESTRICTION_BACKGROUND, ARC can only use tightly restricted
  // CPU resources. The ARC instance is started in a state that is more
  // restricted than CONTAINER_CPU_RESTRICTION_BACKGROUND. When ARC is not
  // supported, the function asynchronously runs the |callback| with false.
  virtual void SetArcCpuRestriction(
      login_manager::ContainerCpuRestrictionState restriction_state,
      VoidDBusMethodCallback callback) = 0;

  // Emits the "arc-booted" upstart signal.
  virtual void EmitArcBooted(const cryptohome::AccountIdentifier& cryptohome_id,
                             VoidDBusMethodCallback callback) = 0;

  // Asynchronously retrieves the timestamp which ARC instance is invoked.
  // Returns nullopt if there is no ARC instance or ARC is not available.
  virtual void GetArcStartTime(
      DBusMethodCallback<base::TimeTicks> callback) = 0;

  using EnableAdbSideloadCallback =
      base::OnceCallback<void(AdbSideloadResponseCode response_code)>;

  // Asynchronously attempts to enable ARC APK Sideloading. Upon completion,
  // invokes |callback| with the result; true on success, false on failure of
  // any kind.
  virtual void EnableAdbSideload(EnableAdbSideloadCallback callback) = 0;

  using QueryAdbSideloadCallback =
      base::OnceCallback<void(AdbSideloadResponseCode response_code,
                              bool is_allowed)>;

  // Asynchronously queries for the current status of ARC APK Sideloading. Upon
  // completion, invokes |callback| with |succeeded| indicating if the query
  // could be completed. If |succeeded| is true, |is_allowed| contains the
  // current status of whether ARC APK Sideloading is allowed on this device,
  // based on previous explicit user opt-in.
  virtual void QueryAdbSideload(QueryAdbSideloadCallback callback) = 0;

 protected:
  // Use Initialize/Shutdown instead.
  SessionManagerClient();
  virtual ~SessionManagerClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionManagerClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SESSION_MANAGER_SESSION_MANAGER_CLIENT_H_
