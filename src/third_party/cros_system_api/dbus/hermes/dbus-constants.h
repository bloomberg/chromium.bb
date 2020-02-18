// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYSTEM_API_DBUS_HERMES_DBUS_CONSTANTS_H_
#define SYSTEM_API_DBUS_HERMES_DBUS_CONSTANTS_H_

namespace hermes {

const char kHermesInterface[] = "org.chromium.Hermes";
const char kHermesServicePath[] = "/org/chromium/Hermes";
const char kHermesServiceName[] = "org.chromium.Hermes";

// Methods.
const char kInstallProfile[] = "InstallProfile";
const char kUninstallProfile[] = "UninstallProfile";
const char kEnableProfile[] = "EnableProfile";
const char kDisableProfile[] = "DisableProfile";
const char kSetProfileNickname[] = "SetProfileNickname";
const char kGetInstalledProfiles[] = "GetInstalledProfiles";
const char kSetTestMode[] = "SetTestMode";

}  // namespace hermes

#endif  // SYSTEM_API_DBUS_HERMES_DBUS_CONSTANTS_H_
