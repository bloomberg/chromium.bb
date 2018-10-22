// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_UTIL_VERSION_LOADER_H_
#define CHROMEOS_DBUS_UTIL_VERSION_LOADER_H_

#include <string>

#include "base/callback_forward.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/cryptohome_client.h"

namespace chromeos {
namespace version_loader {

enum VersionFormat {
  VERSION_SHORT,
  VERSION_SHORT_WITH_DATE,
  VERSION_FULL,
};

using GetTpmVersionCallback = base::OnceCallback<void(
    const CryptohomeClient::TpmVersionInfo& tpm_version_info)>;

// Gets the version.
// If |full_version| is true version string with extra info is extracted,
// otherwise it's in short format x.x.xx.x.
// May block.
CHROMEOS_EXPORT std::string GetVersion(VersionFormat format);

// Gets the TPM version information. Asynchronous, result is passed on to
// callback.
CHROMEOS_EXPORT void GetTpmVersion(GetTpmVersionCallback callback);

// Gets the ARC version.
// May block.
CHROMEOS_EXPORT std::string GetARCVersion();

// Gets the firmware info.
// May block.
CHROMEOS_EXPORT std::string GetFirmware();

// Extracts the firmware from the file.
CHROMEOS_EXPORT std::string ParseFirmware(const std::string& contents);

// Returns true if |new_version| is older than |current_version|.
// Version numbers should be dot separated. The sections are compared as
// numbers if possible, as strings otherwise. Earlier sections have
// precedence. If one version is prefix of another, the shorter one is
// considered older. (See test for examples.)
CHROMEOS_EXPORT bool IsRollback(const std::string& current_version,
                                const std::string& new_version);

}  // namespace version_loader
}  // namespace chromeos

#endif  // CHROMEOS_DBUS_UTIL_VERSION_LOADER_H_
