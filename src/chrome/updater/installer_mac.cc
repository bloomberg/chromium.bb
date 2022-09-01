// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/installer.h"

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/mac/install_from_archive.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {

AppInstallerResult RunApplicationInstaller(
    const AppInfo& app_info,
    const base::FilePath& app_installer,
    const std::string& arguments,
    const absl::optional<base::FilePath>& installer_data_file,
    InstallProgressCallback /*progress_callback*/) {
  DVLOG(1) << "Running application install from DMG at " << app_installer;
  // InstallFromArchive() returns the exit code of the script. 0 is success and
  // anything else should be an error.
  const int exit_code = InstallFromArchive(
      app_installer, app_info.ecp, app_info.ap, app_info.scope,
      app_info.version, arguments, installer_data_file);
  return exit_code == 0
             ? AppInstallerResult()
             : AppInstallerResult(kErrorApplicationInstallerFailed, exit_code);
}

}  // namespace updater
