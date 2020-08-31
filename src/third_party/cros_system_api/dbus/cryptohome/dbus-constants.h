// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYSTEM_API_DBUS_CRYPTOHOME_DBUS_CONSTANTS_H_
#define SYSTEM_API_DBUS_CRYPTOHOME_DBUS_CONSTANTS_H_

namespace cryptohome {

// Interface exposed by the cryptohome daemon.

const char kCryptohomeInterface[] = "org.chromium.CryptohomeInterface";
const char kCryptohomeServicePath[] = "/org/chromium/Cryptohome";
const char kCryptohomeServiceName[] = "org.chromium.Cryptohome";

const char kUserDataAuthServiceName[] = "org.chromium.UserDataAuth";
const char kUserDataAuthServicePath[] = "/org/chromium/UserDataAuth";

const char kUserDataAuthInterface[] = "org.chromium.UserDataAuthInterface";
const char kCryptohomePkcs11Interface[] =
    "org.chromium.CryptohomePkcs11Interface";
const char kInstallAttributesInterface[] =
    "org.chromium.InstallAttributesInterface";
const char kCryptohomeMiscInterface[] = "org.chromium.CryptohomeMiscInterface";

// Methods of the |kCryptohomeInterface| interface:
const char kCryptohomeMigrateKey[] = "MigrateKey";
const char kCryptohomeMigrateKeyEx[] = "MigrateKeyEx";
const char kCryptohomeRemoveEx[] = "RemoveEx";
const char kCryptohomeGetSystemSalt[] = "GetSystemSalt";
const char kCryptohomeGetSanitizedUsername[] = "GetSanitizedUsername";
const char kCryptohomeIsMounted[] = "IsMounted";
const char kCryptohomeMount[] = "Mount";
const char kCryptohomeMountGuest[] = "MountGuest";
const char kCryptohomeMountGuestEx[] = "MountGuestEx";
const char kCryptohomeUnmountEx[] = "UnmountEx";
const char kCryptohomeTpmIsReady[] = "TpmIsReady";
const char kCryptohomeTpmIsEnabled[] = "TpmIsEnabled";
const char kCryptohomeTpmIsOwned[] = "TpmIsOwned";
const char kCryptohomeTpmIsBeingOwned[] = "TpmIsBeingOwned";
const char kCryptohomeTpmGetPassword[] = "TpmGetPassword";
const char kCryptohomeTpmCanAttemptOwnership[] = "TpmCanAttemptOwnership";
const char kCryptohomeTpmClearStoredPassword[] = "TpmClearStoredPassword";
const char kCryptohomePkcs11GetTpmTokenInfo[] = "Pkcs11GetTpmTokenInfo";
const char kCryptohomePkcs11GetTpmTokenInfoForUser[] =
    "Pkcs11GetTpmTokenInfoForUser";
const char kCryptohomePkcs11IsTpmTokenReady[] = "Pkcs11IsTpmTokenReady";
const char kCryptohomeAsyncMigrateKey[] = "AsyncMigrateKey";
const char kCryptohomeAsyncMount[] = "AsyncMount";
const char kCryptohomeAsyncMountGuest[] = "AsyncMountGuest";
const char kCryptohomeAsyncRemove[] = "AsyncRemove";
const char kCryptohomeGetStatusString[] = "GetStatusString";
const char kCryptohomeRemoveTrackedSubdirectories[] =
    "RemoveTrackedSubdirectories";
const char kCryptohomeAsyncRemoveTrackedSubdirectories[] =
    "AsyncRemoveTrackedSubdirectories";
const char kCryptohomeAsyncDoesUsersExist[] = "AsyncDoesUsersExist";
const char kCryptohomeInstallAttributesGet[] = "InstallAttributesGet";
const char kCryptohomeInstallAttributesSet[] = "InstallAttributesSet";
const char kCryptohomeInstallAttributesCount[] = "InstallAttributesCount";
const char kCryptohomeInstallAttributesFinalize[] =
    "InstallAttributesFinalize";
const char kCryptohomeInstallAttributesIsReady[] = "InstallAttributesIsReady";
const char kCryptohomeInstallAttributesIsSecure[] =
    "InstallAttributesIsSecure";
const char kCryptohomeInstallAttributesIsInvalid[] =
    "InstallAttributesIsInvalid";
const char kCryptohomeInstallAttributesIsFirstInstall[] =
    "InstallAttributesIsFirstInstall";
const char kCryptohomeTpmIsAttestationPrepared[] = "TpmIsAttestationPrepared";
const char kCryptohomeTpmAttestationGetEnrollmentPreparationsEx[] =
    "TpmAttestationGetEnrollmentPreparationsEx";
const char kCryptohomeTpmIsAttestationEnrolled[] = "TpmIsAttestationEnrolled";
const char kCryptohomeTpmAttestationGetIdentityCertificatesEx[] =
    "TpmAttestationGetIdentityCertificatesEx";
const char kCryptohomeTpmAttestationCreateEnrollRequest[] =
    "TpmAttestationCreateEnrollRequest";
const char kCryptohomeAsyncTpmAttestationCreateEnrollRequest[] =
    "AsyncTpmAttestationCreateEnrollRequest";
const char kCryptohomeAsyncTpmAttestationCreateEnrollRequestNew[] =
    "AsyncTpmAttestationCreateEnrollRequestNew";
const char kCryptohomeTpmAttestationEnroll[] = "TpmAttestationEnroll";
const char kCryptohomeAsyncTpmAttestationEnroll[] = "AsyncTpmAttestationEnroll";
const char kCryptohomeAsyncTpmAttestationEnrollNew[] =
    "AsyncTpmAttestationEnrollNew";
const char kCryptohomeTpmAttestationCreateCertRequest[] =
    "TpmAttestationCreateCertRequest";
const char kCryptohomeAsyncTpmAttestationCreateCertRequest[] =
    "AsyncTpmAttestationCreateCertRequest";
const char kCryptohomeAsyncTpmAttestationCreateCertRequestByProfile[] =
    "AsyncTpmAttestationCreateCertRequestByProfile";
const char kCryptohomeTpmAttestationFinishCertRequest[] =
    "TpmAttestationFinishCertRequest";
const char kCryptohomeAsyncTpmAttestationFinishCertRequest[] =
    "AsyncTpmAttestationFinishCertRequest";
const char kCryptohomeTpmAttestationDoesKeyExist[] =
    "TpmAttestationDoesKeyExist";
const char kCryptohomeTpmAttestationGetCertificate[] =
    "TpmAttestationGetCertificate";
const char kCryptohomeTpmAttestationGetPublicKey[] =
    "TpmAttestationGetPublicKey";
const char kCryptohomeTpmAttestationRegisterKey[] = "TpmAttestationRegisterKey";
// TODO(crbug.com/789419): Remove this deprecated API.
const char kCryptohomeTpmAttestationSignEnterpriseChallenge[] =
    "TpmAttestationSignEnterpriseChallenge";
const char kCryptohomeTpmAttestationSignEnterpriseVaChallenge[] =
    "TpmAttestationSignEnterpriseVaChallenge";
// TODO(crbug.com/988367,b/35580115): This temporary method is used to change
// the signature of |kCryptohomeTpmAttestationSignEnterpriseVaChallenge| to
// accept a new argument. The plan is to migrate this to a function that takes
// a protobuf for easier interface changes in the future. This method will be
// removed when tha tis done.
const char kCryptohomeTpmAttestationSignEnterpriseVaChallengeV2[] =
    "TpmAttestationSignEnterpriseVaChallengeV2";
const char kCryptohomeTpmAttestationSignSimpleChallenge[] =
    "TpmAttestationSignSimpleChallenge";
const char kCryptohomeTpmAttestationGetKeyPayload[] =
    "TpmAttestationGetKeyPayload";
const char kCryptohomeTpmAttestationSetKeyPayload[] =
    "TpmAttestationSetKeyPayload";
const char kCryptohomeTpmAttestationDeleteKey[] =
    "TpmAttestationDeleteKey";
const char kCryptohomeTpmAttestationDeleteKeys[] =
    "TpmAttestationDeleteKeys";
const char kCryptohomeTpmAttestationGetEnrollmentId[] =
    "TpmAttestationGetEnrollmentId";
const char kCryptohomeTpmGetVersionStructured[] = "TpmGetVersionStructured";
const char kCryptohomeGetKeyDataEx[] = "GetKeyDataEx";
const char kCryptohomeCheckKeyEx[] = "CheckKeyEx";
const char kCryptohomeMountEx[] = "MountEx";
const char kCryptohomeAddKeyEx[] = "AddKeyEx";
const char kCryptohomeUpdateKeyEx[] = "UpdateKeyEx";
const char kCryptohomeRemoveKeyEx[] = "RemoveKeyEx";
const char kCryptohomeAddDataRestoreKey[] = "AddDataRestoreKey";
const char kCryptohomeMassRemoveKeys[] = "MassRemoveKeys";
const char kCryptohomeSignBootLockbox[] = "SignBootLockbox";
const char kCryptohomeVerifyBootLockbox[] = "VerifyBootLockbox";
const char kCryptohomeFinalizeBootLockbox[] = "FinalizeBootLockbox";
const char kCryptohomeGetBootAttribute[] = "GetBootAttribute";
const char kCryptohomeSetBootAttribute[] = "SetBootAttribute";
const char kCryptohomeFlushAndSignBootAttributes[] =
    "FlushAndSignBootAttributes";
const char kCryptohomeGetLoginStatus[] = "GetLoginStatus";
const char kCryptohomeGetTpmStatus[] = "GetTpmStatus";
const char kCryptohomeGetEndorsementInfo[] = "GetEndorsementInfo";
const char kCryptohomeRenameCryptohome[] = "RenameCryptohome";
const char kCryptohomeGetAccountDiskUsage[] = "GetAccountDiskUsage";
const char kCryptohomeGetFirmwareManagementParameters[] =
    "GetFirmwareManagementParameters";
const char kCryptohomeSetFirmwareManagementParameters[] =
    "SetFirmwareManagementParameters";
const char kCryptohomeRemoveFirmwareManagementParameters[] =
    "RemoveFirmwareManagementParameters";
const char kCryptohomeMigrateToDircrypto[] = "MigrateToDircrypto";
const char kCryptohomeNeedsDircryptoMigration[] = "NeedsDircryptoMigration";
const char kCryptohomeGetSupportedKeyPolicies[] = "GetSupportedKeyPolicies";
const char kCryptohomeIsQuotaSupported[] = "IsQuotaSupported";
const char kCryptohomeGetCurrentSpaceForUid[] = "GetCurrentSpaceForUid";
const char kCryptohomeGetCurrentSpaceForGid[] = "GetCurrentSpaceForGid";
const char kCryptohomeLockToSingleUserMountUntilReboot[] =
    "LockToSingleUserMountUntilReboot";
const char kCryptohomeGetRsuDeviceId[] = "GetRsuDeviceId";
const char kCryptohomeCheckHealth[] = "CheckHealth";

// Signals of the |kCryptohomeInterface| interface:
const char kSignalAsyncCallStatus[] = "AsyncCallStatus";
const char kSignalAsyncCallStatusWithData[] = "AsyncCallStatusWithData";
const char kSignalTpmInitStatus[] = "TpmInitStatus";
const char kSignalCleanupUsersRemoved[] = "CleanupUsersRemoved";
const char kSignalLowDiskSpace[] = "LowDiskSpace";
const char kSignalDircryptoMigrationProgress[] = "DircryptoMigrationProgress";

// Error code
enum MountError {
  MOUNT_ERROR_NONE = 0,
  MOUNT_ERROR_FATAL = 1,
  MOUNT_ERROR_KEY_FAILURE = 2,
  MOUNT_ERROR_INVALID_ARGS = 3,
  MOUNT_ERROR_MOUNT_POINT_BUSY = 4,
  MOUNT_ERROR_EPHEMERAL_MOUNT_BY_OWNER = 5,
  MOUNT_ERROR_CREATE_CRYPTOHOME_FAILED = 6,
  MOUNT_ERROR_REMOVE_INVALID_USER_FAILED = 7,
  MOUNT_ERROR_TPM_COMM_ERROR = 8,
  MOUNT_ERROR_UNPRIVILEGED_KEY = 9,
  MOUNT_ERROR_SETUP_PROCESS_KEYRING_FAILED = 10,
  MOUNT_ERROR_UNEXPECTED_MOUNT_TYPE = 11,
  MOUNT_ERROR_KEYRING_FAILED = 12,
  MOUNT_ERROR_DIR_CREATION_FAILED = 13,
  MOUNT_ERROR_SET_DIR_CRYPTO_KEY_FAILED = 14,
  MOUNT_ERROR_MOUNT_ECRYPTFS_FAILED = 15,
  MOUNT_ERROR_TPM_DEFEND_LOCK = 16,
  MOUNT_ERROR_SETUP_GROUP_ACCESS_FAILED = 17,
  MOUNT_ERROR_MOUNT_HOMES_AND_DAEMON_STORES_FAILED = 18,
  MOUNT_ERROR_TPM_UPDATE_REQUIRED = 19,
  MOUNT_ERROR_USER_DOES_NOT_EXIST = 32,
  MOUNT_ERROR_TPM_NEEDS_REBOOT = 64,
  // Encrypted in old method, need migration before mounting.
  MOUNT_ERROR_OLD_ENCRYPTION = 128,
  // Previous migration attempt was aborted in the middle. Must resume it first.
  MOUNT_ERROR_PREVIOUS_MIGRATION_INCOMPLETE = 256,
  // The operation to remove a key failed.
  MOUNT_ERROR_REMOVE_FAILED = 512,
  MOUNT_ERROR_RECREATED = 1 << 31,
};
// Status code signaled from MigrateToDircrypto().
enum DircryptoMigrationStatus {
  // 0 means a successful completion.
  DIRCRYPTO_MIGRATION_SUCCESS = 0,
  // Negative values mean failing completion.
  // TODO(kinaba,dspaid): Add error codes as needed here.
  DIRCRYPTO_MIGRATION_FAILED = -1,
  // Positive values mean intermediate state report for the running migration.
  // TODO(kinaba,dspaid): Add state codes as needed.
  DIRCRYPTO_MIGRATION_INITIALIZING = 1,
  DIRCRYPTO_MIGRATION_IN_PROGRESS = 2,
};

// Interface for key delegate service to be used by the cryptohome daemon.

const char kCryptohomeKeyDelegateInterface[] =
    "org.chromium.CryptohomeKeyDelegateInterface";

// Methods of the |kCryptohomeKeyDelegateInterface| interface:
const char kCryptohomeKeyDelegateChallengeKey[] = "ChallengeKey";

}  // namespace cryptohome

#endif  // SYSTEM_API_DBUS_CRYPTOHOME_DBUS_CONSTANTS_H_
