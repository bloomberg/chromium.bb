// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_MAC_INSTALL_FROM_ARCHIVE_H_
#define CHROME_UPDATER_MAC_INSTALL_FROM_ARCHIVE_H_

#include <string>

namespace base {
class FilePath;
class Version;
}

namespace updater {

enum class UpdaterScope;

enum class InstallErrors {
  // Failed to mount the DMG.
  kFailMountDmg = -1,

  // No mount point was created from the DMG, even though mounting succeeded.
  kNoMountPoint = -2,

  // Failed to find the mounted DMG path, even though mounting succeeded and a
  // mount point was created.
  kMountedDmgPathDoesNotExist = -3,

  // Failed to find a path to the install executable.
  kExecutableFilePathDoesNotExist = -4,

  // Executable path does not contain an executable file.
  kExecutablePathNotExecutable = -5,

  // Zip file failed to expand.
  kFailedToExpandZip = -6,

  // The installer type given by the run command is not valid.
  kNotSupportedInstallerType = -7,

  // Correct permissions could not be validated for the install path.
  kCouldNotConfirmAppPermissions = -8,

  // An install executable was signaled or timed out.
  kExecutableWaitForExitFailed = -9,
};

// Choose which type of archive to install from. Possible types of archives are
// DMG, Zip and just the App. From there, it calls the archive specific
// installation method.
int InstallFromArchive(const base::FilePath& file_path,
                       const base::FilePath& existence_checker_path,
                       const std::string& ap,
                       const UpdaterScope& scope,
                       const base::Version& pv,
                       const std::string& arguments);

}  // namespace updater

#endif  // CHROME_UPDATER_MAC_INSTALL_FROM_ARCHIVE_H_
