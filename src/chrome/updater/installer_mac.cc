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

namespace updater {

Installer::Result Installer::RunApplicationInstaller(
    const base::FilePath& app_installer,
    const std::string& arguments,
    ProgressCallback /*progress_callback*/) {
  DVLOG(1) << "Running application install from DMG at " << app_installer;
  // InstallFromArchive() returns the exit code of the script. 0 is success and
  // anything else should be an error.
  const int exit_code = InstallFromArchive(app_installer, checker_path_, ap_,
                                           updater_scope_, pv_, arguments);
  return exit_code == 0 ? Result()
                        : Result(kErrorApplicationInstallerFailed, exit_code);
}

}  // namespace updater
