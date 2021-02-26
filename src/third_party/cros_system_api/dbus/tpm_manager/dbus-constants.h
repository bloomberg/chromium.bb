// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYSTEM_API_DBUS_TPM_MANAGER_DBUS_CONSTANTS_H_
#define SYSTEM_API_DBUS_TPM_MANAGER_DBUS_CONSTANTS_H_

namespace tpm_manager {

// D-Bus service constants.
constexpr char kTpmManagerInterface[] = "org.chromium.TpmManager";
constexpr char kTpmManagerServiceName[] = "org.chromium.TpmManager";
constexpr char kTpmManagerServicePath[] = "/org/chromium/TpmManager";

// Methods exported by tpm_manager.
constexpr char kGetTpmStatus[] = "GetTpmStatus";
constexpr char kGetTpmNonsensitiveStatus[] = "GetTpmNonsensitiveStatus";
constexpr char kGetVersionInfo[] = "GetVersionInfo";
constexpr char kGetDictionaryAttackInfo[] = "GetDictionaryAttackInfo";
constexpr char kResetDictionaryAttackLock[] = "ResetDictionaryAttackLock";
constexpr char kTakeOwnership[] = "TakeOwnership";
constexpr char kRemoveOwnerDependency[] = "RemoveOwnerDependency";
constexpr char kClearStoredOwnerPassword[] = "ClearStoredOwnerPassword";

// Signal registered by tpm_manager ownership D-Bus interface.
constexpr char kOwnershipTakenSignal[] = "SignalOwnershipTaken";

// Default dependencies on TPM owner privilege. The TPM owner password will not
// be destroyed until all of these dependencies have been explicitly removed
// using the RemoveOwnerDependency method.
constexpr const char* kTpmOwnerDependency_Nvram = "TpmOwnerDependency_Nvram";
constexpr const char* kTpmOwnerDependency_Attestation =
    "TpmOwnerDependency_Attestation";

constexpr const char* kInitialTpmOwnerDependencies[] = {
    kTpmOwnerDependency_Nvram, kTpmOwnerDependency_Attestation};

}  // namespace tpm_manager

#endif  // SYSTEM_API_DBUS_TPM_MANAGER_DBUS_CONSTANTS_H_
