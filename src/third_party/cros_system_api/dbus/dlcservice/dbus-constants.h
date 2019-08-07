// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYSTEM_API_DBUS_DLCSERVICE_DBUS_CONSTANTS_H_
#define SYSTEM_API_DBUS_DLCSERVICE_DBUS_CONSTANTS_H_

namespace dlcservice {

const char kDlcServiceInterface[] = "org.chromium.DlcServiceInterface";
const char kDlcServiceServicePath[] = "/org/chromium/DlcService";
const char kDlcServiceServiceName[] = "org.chromium.DlcService";

const char kGetInstalledMethod[] = "GetInstalled";
const char kInstallMethod[] = "Install";
const char kUninstallMethod[] = "Uninstall";

enum class OnInstalledSignalErrorCode {
  kNone = 0,
  kUnknown = 1,
  kImageLoaderReturnsFalse = 2,
  kMountFailure = 3,
};

}  // namespace dlcservice

#endif  // SYSTEM_API_DBUS_DLCSERVICE_DBUS_CONSTANTS_H_
