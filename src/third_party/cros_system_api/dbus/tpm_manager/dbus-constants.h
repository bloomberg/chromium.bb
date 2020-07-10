//
// Copyright (C) 2015 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

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
