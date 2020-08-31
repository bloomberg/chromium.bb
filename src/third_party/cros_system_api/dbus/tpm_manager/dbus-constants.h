// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYSTEM_API_DBUS_TPM_MANAGER_DBUS_CONSTANTS_H_
#define SYSTEM_API_DBUS_TPM_MANAGER_DBUS_CONSTANTS_H_

namespace tpm_manager {

// D-Bus service constants.
constexpr char kTpmManagerServiceName[] = "org.chromium.TpmManager";
constexpr char kTpmManagerServicePath[] = "/org/chromium/TpmManager";

// Binder service constants.
constexpr char kTpmNvramBinderName[] = "android.tpm_manager.ITpmNvram";
constexpr char kTpmOwnershipBinderName[] = "android.tpm_manager.ITpmOwnership";

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
