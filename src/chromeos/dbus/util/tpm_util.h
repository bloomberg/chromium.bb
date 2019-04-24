// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_UTIL_TPM_UTIL_H_
#define CHROMEOS_DBUS_UTIL_TPM_UTIL_H_

#include <string>

#include "base/component_export.h"

namespace chromeos {

// Wrappers of the D-Bus method calls for working with Tpm.  Note that all of
// these are blocking and thus must not be called on the UI thread!
namespace tpm_util {

// Returns whether Tpm is presented and enabled.
COMPONENT_EXPORT(CHROMEOS_DBUS) bool TpmIsEnabled();

// Returns whether device has already been owned.
COMPONENT_EXPORT(CHROMEOS_DBUS) bool TpmIsOwned();

// Returns whether device is being owned (Tpm password is generating).
COMPONENT_EXPORT(CHROMEOS_DBUS) bool TpmIsBeingOwned();

COMPONENT_EXPORT(CHROMEOS_DBUS)
bool InstallAttributesGet(const std::string& name, std::string* value);
COMPONENT_EXPORT(CHROMEOS_DBUS)
bool InstallAttributesSet(const std::string& name, const std::string& value);
COMPONENT_EXPORT(CHROMEOS_DBUS) bool InstallAttributesFinalize();
COMPONENT_EXPORT(CHROMEOS_DBUS) bool InstallAttributesIsInvalid();
COMPONENT_EXPORT(CHROMEOS_DBUS) bool InstallAttributesIsFirstInstall();

// Checks if device is locked for Active Directory management.
COMPONENT_EXPORT(CHROMEOS_DBUS) bool IsActiveDirectoryLocked();

// Sets install attributes for an Active Directory managed device and persists
// them on disk.
COMPONENT_EXPORT(CHROMEOS_DBUS)
bool LockDeviceActiveDirectoryForTesting(const std::string& realm);

}  // namespace tpm_util
}  // namespace chromeos

#endif  // CHROMEOS_DBUS_UTIL_TPM_UTIL_H_
