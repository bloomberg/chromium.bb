// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_USERDATAAUTH_FAKE_USERDATAAUTH_CLIENT_H_
#define CHROMEOS_DBUS_USERDATAAUTH_FAKE_USERDATAAUTH_CLIENT_H_

#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/userdataauth/userdataauth_client.h"

#include <set>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/dbus/cryptohome/account_identifier_operators.h"

namespace chromeos {

class COMPONENT_EXPORT(USERDATAAUTH_CLIENT) FakeUserDataAuthClient
    : public UserDataAuthClient {
 public:
  // Represents the ongoing AuthSessions.
  struct AuthSessionData {
    // AuthSession id.
    std::string id;
    // Account associated with the session.
    cryptohome::AccountIdentifier account;
    // True if session is authenticated.
    bool authenticated = false;
  };

  FakeUserDataAuthClient();
  ~FakeUserDataAuthClient() override;

  // Not copyable or movable.
  FakeUserDataAuthClient(const FakeUserDataAuthClient&) = delete;
  FakeUserDataAuthClient& operator=(const FakeUserDataAuthClient&) = delete;

  // Checks that a FakeUserDataAuthClient instance was initialized and returns
  // it.
  static FakeUserDataAuthClient* Get();

  // UserDataAuthClient override:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void WaitForServiceToBeAvailable(
      chromeos::WaitForServiceToBeAvailableCallback callback) override;
  void IsMounted(const ::user_data_auth::IsMountedRequest& request,
                 IsMountedCallback callback) override;
  void Unmount(const ::user_data_auth::UnmountRequest& request,
               UnmountCallback callback) override;
  void Mount(const ::user_data_auth::MountRequest& request,
             MountCallback callback) override;
  void Remove(const ::user_data_auth::RemoveRequest& request,
              RemoveCallback callback) override;
  void GetKeyData(const ::user_data_auth::GetKeyDataRequest& request,
                  GetKeyDataCallback callback) override;
  void CheckKey(const ::user_data_auth::CheckKeyRequest& request,
                CheckKeyCallback callback) override;
  void AddKey(const ::user_data_auth::AddKeyRequest& request,
              AddKeyCallback callback) override;
  void RemoveKey(const ::user_data_auth::RemoveKeyRequest& request,
                 RemoveKeyCallback callback) override;
  void MassRemoveKeys(const ::user_data_auth::MassRemoveKeysRequest& request,
                      MassRemoveKeysCallback callback) override;
  void MigrateKey(const ::user_data_auth::MigrateKeyRequest& request,
                  MigrateKeyCallback callback) override;
  void StartFingerprintAuthSession(
      const ::user_data_auth::StartFingerprintAuthSessionRequest& request,
      StartFingerprintAuthSessionCallback callback) override;
  void EndFingerprintAuthSession(
      const ::user_data_auth::EndFingerprintAuthSessionRequest& request,
      EndFingerprintAuthSessionCallback callback) override;
  void StartMigrateToDircrypto(
      const ::user_data_auth::StartMigrateToDircryptoRequest& request,
      StartMigrateToDircryptoCallback callback) override;
  void NeedsDircryptoMigration(
      const ::user_data_auth::NeedsDircryptoMigrationRequest& request,
      NeedsDircryptoMigrationCallback callback) override;
  void GetSupportedKeyPolicies(
      const ::user_data_auth::GetSupportedKeyPoliciesRequest& request,
      GetSupportedKeyPoliciesCallback callback) override;
  void GetAccountDiskUsage(
      const ::user_data_auth::GetAccountDiskUsageRequest& request,
      GetAccountDiskUsageCallback callback) override;
  void StartAuthSession(
      const ::user_data_auth::StartAuthSessionRequest& request,
      StartAuthSessionCallback callback) override;
  void AuthenticateAuthSession(
      const ::user_data_auth::AuthenticateAuthSessionRequest& request,
      AuthenticateAuthSessionCallback callback) override;
  void AddCredentials(const ::user_data_auth::AddCredentialsRequest& request,
                      AddCredentialsCallback callback) override;

  // Mount() related setter/getters.

  // Sets the CryptohomeError value to return.
  void set_cryptohome_error(::user_data_auth::CryptohomeErrorCode error) {
    cryptohome_error_ = error;
  }
  // Get the MountRequest to last Mount().
  const ::user_data_auth::MountRequest& get_last_mount_request() const {
    return last_mount_request_;
  }
  // If the last call to Mount() have to_migrate_from_ecryptfs set.
  bool to_migrate_from_ecryptfs() const {
    return last_mount_request_.to_migrate_from_ecryptfs();
  }
  // If the last call to Mount() have public_mount set.
  bool public_mount() const { return last_mount_request_.public_mount(); }
  // Return the authorization request passed to the last Mount().
  const cryptohome::AuthorizationRequest& get_last_mount_authentication()
      const {
    return last_mount_request_.authorization();
  }
  // Return the secret passed to last Mount().
  const std::string& get_secret_for_last_mount_authentication() const {
    return last_mount_request_.authorization().key().secret();
  }
  // Sets whether the Mount() call should fail when the |create| field is not
  // provided (the error code will be CRYPTOHOME_ERROR_ACCOUNT_NOT_FOUND).
  // This allows to simulate the behavior during the new user profile creation.
  void set_mount_create_required(bool mount_create_required) {
    mount_create_required_ = mount_create_required;
  }

  // Key related:

  // If enable_auth_check is true, then CheckKey will actually check the
  // authorization.
  void set_enable_auth_check(bool enable_auth_check) {
    enable_auth_check_ = enable_auth_check;
  }

  // If set, next call to GetSupportedKeyPolicies() will tell caller that low
  // entropy credentials are supported.
  void set_supports_low_entropy_credentials(bool supports) {
    supports_low_entropy_credentials_ = supports;
  }

  // eCryptfs related:

  // Marks |cryptohome_id| as using ecryptfs (|use_ecryptfs|=true) or
  // dircrypto
  // (|use_ecryptfs|=false).
  void SetEcryptfsUserHome(const cryptohome::AccountIdentifier& cryptohome_id,
                           bool use_ecryptfs);

  // Sets whether dircrypto migration update should be run automatically.
  // If set to false, the client will not send any dircrypto migration progress
  // updates on its own - a test that sets this will have to call
  // NotifyDircryptoMigrationProgress() for the progress to update.
  void set_run_default_dircrypto_migration(bool value) {
    run_default_dircrypto_migration_ = value;
  }

  // Getter for the AccountIdentifier() that was passed to the last
  // StartMigrateToDircrypto() call.
  const cryptohome::AccountIdentifier& get_id_for_disk_migrated_to_dircrypto()
      const {
    return last_migrate_to_dircrypto_request_.account_id();
  }
  // Whether the last StartMigrateToDircrypto() call indicates minimal
  // migration.
  bool minimal_migration() const {
    return last_migrate_to_dircrypto_request_.minimal_migration();
  }

  // AuthenticateAuthSession() related:
  const ::cryptohome::AuthorizationRequest&
  get_last_authenticate_auth_session_authorization() {
    return last_authenticate_auth_session_request_.authorization();
  }

  // WaitForServiceToBeAvailable() related:

  // Changes the behavior of WaitForServiceToBeAvailable(). This method runs
  // pending callbacks if is_available is true.
  void SetServiceIsAvailable(bool is_available);

  // Runs pending availability callbacks reporting that the service is
  // unavailable. Expects service not to be available when called.
  void ReportServiceIsNotAvailable();

  // Signal related:

  // Calls DircryptoMigrationProgress() on Observer instances.
  void NotifyDircryptoMigrationProgress(
      ::user_data_auth::DircryptoMigrationStatus status,
      uint64_t current,
      uint64_t total);

  // Calls LowDiskSpace() on Observer instances.
  void NotifyLowDiskSpace(uint64_t disk_free_bytes);

  void AddExistingUser(const cryptohome::AccountIdentifier& account_id);

 private:
  // Helper that returns the protobuf reply.
  template <typename ReplyType>
  void ReturnProtobufMethodCallback(const ReplyType& reply,
                                    DBusMethodCallback<ReplyType> callback);

  // Returns true if the user's home is backed by eCryptfs.
  bool IsEcryptfsUserHome(const cryptohome::AccountIdentifier& cryptohome_id);

  // This method is used to implement StartMigrateToDircrypto with simulated
  // progress updates.
  void OnDircryptoMigrationProgressUpdated();

  // Finds a key matching the given label. Wildcard labels are supported.
  std::map<std::string, cryptohome::Key>::const_iterator FindKey(
      const std::map<std::string, cryptohome::Key>& keys,
      const std::string& label);

  // Check whether user with given id exists
  bool UserExists(const cryptohome::AccountIdentifier& account_id) const;

  // Mount() related fields.
  ::user_data_auth::CryptohomeErrorCode cryptohome_error_ =
      ::user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET;
  ::user_data_auth::MountRequest last_mount_request_;
  bool mount_create_required_ = false;

  // Key related fields.

  // Controls if CheckKeyEx actually checks the key.
  bool enable_auth_check_ = false;

  // The key data for various accounts.
  std::map<cryptohome::AccountIdentifier,
           std::map<std::string, cryptohome::Key>>
      key_data_map_;

  // If low entropy credentials are supported for the key. This is the value
  // that GetSupportedKeyPolicies() returns.
  bool supports_low_entropy_credentials_ = false;

  // eCryptfs/dircrypto migration related:

  // Set of account identifiers whose user homes use ecryptfs. User homes not
  // mentioned here use dircrypto.
  std::set<cryptohome::AccountIdentifier> ecryptfs_user_homes_;

  // Timer for triggering the dircrypto migration progress signal.
  base::RepeatingTimer dircrypto_migration_progress_timer_;

  // The current dircrypto migration progress indicator, used when we trigger
  // the migration progress signal.
  uint64_t dircrypto_migration_progress_ = 0;

  // Do we run the dircrypto migration, as in, emit signals, when
  // StartMigrateToDircrypto() is called?
  bool run_default_dircrypto_migration_ = true;

  // The StartMigrateToDircryptoRequest passed in for the last
  // StartMigrateToDircrypto() call.
  ::user_data_auth::StartMigrateToDircryptoRequest
      last_migrate_to_dircrypto_request_;

  // AuthSession related fields:

  // The AuthenticateAuthSessionRequest passed in for the last
  // AuthenticateAuthSession() call.
  ::user_data_auth::AuthenticateAuthSessionRequest
      last_authenticate_auth_session_request_;

  // The auth sessions on file.
  base::flat_map<std::string, AuthSessionData> auth_sessions_;

  // Next available auth session id.
  int next_auth_session_id_ = 0;

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

  // Other stuff/miscellaneous:

  // The users that have already logged in at least once
  std::set<cryptohome::AccountIdentifier> existing_users_;

  // List of observers.
  base::ObserverList<Observer> observer_list_;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::FakeUserDataAuthClient;
}

#endif  // CHROMEOS_DBUS_USERDATAAUTH_FAKE_USERDATAAUTH_CLIENT_H_
