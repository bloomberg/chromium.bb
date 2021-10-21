// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/util.h"

#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/strings/strcat.h"
#include "base/version.h"
#include "chrome/updater/constants.h"
#import "chrome/updater/mac/mac_util.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_version.h"

namespace updater {
namespace {

constexpr base::FilePath::CharType kZipExePath[] =
    FILE_PATH_LITERAL("/usr/bin/unzip");

base::FilePath GetUpdaterFolderName() {
  return base::FilePath(COMPANY_SHORTNAME_STRING)
      .AppendASCII(PRODUCT_FULLNAME_STRING);
}

base::FilePath ExecutableFolderPath() {
  return base::FilePath(
             base::StrCat({PRODUCT_FULLNAME_STRING, kExecutableSuffix, ".app"}))
      .Append(FILE_PATH_LITERAL("Contents"))
      .Append(FILE_PATH_LITERAL("MacOS"));
}
}  // namespace

bool UnzipWithExe(const base::FilePath& src_path,
                  const base::FilePath& dest_path) {
  base::FilePath file_path(kZipExePath);
  base::CommandLine command(file_path);
  command.AppendArg(src_path.value());
  command.AppendArg("-d");
  command.AppendArg(dest_path.value());

  std::string output;
  int exit_code = 0;
  if (!base::GetAppOutputWithExitCode(command, &output, &exit_code)) {
    VLOG(0) << "Something went wrong while running the unzipping with "
            << kZipExePath;
    return false;
  }

  // Unzip utility having 0 is success and 1 is a warning.
  if (exit_code > 1) {
    VLOG(0) << "Output from unzipping: " << output;
    VLOG(0) << "Exit code: " << exit_code;
  }

  return exit_code <= 1;
}

absl::optional<base::FilePath> GetUpdaterFolderPath(UpdaterScope scope) {
  absl::optional<base::FilePath> path = GetLibraryFolderPath(scope);
  if (!path)
    return absl::nullopt;
  return path->Append(GetUpdaterFolderName());
}

absl::optional<base::FilePath> GetExecutableFolderPathForVersion(
    UpdaterScope scope,
    const base::Version& version) {
  absl::optional<base::FilePath> path =
      GetVersionedUpdaterFolderPathForVersion(scope, version);
  if (!path)
    return absl::nullopt;
  return path->Append(ExecutableFolderPath());
}

absl::optional<base::FilePath> GetUpdaterAppBundlePath(UpdaterScope scope) {
  absl::optional<base::FilePath> path = GetVersionedUpdaterFolderPath(scope);
  if (!path)
    return absl::nullopt;
  return path->Append(
      base::StrCat({PRODUCT_FULLNAME_STRING, kExecutableSuffix, ".app"}));
}

absl::optional<base::FilePath> GetUpdaterExecutablePath(UpdaterScope scope) {
  absl::optional<base::FilePath> path = GetVersionedUpdaterFolderPath(scope);
  if (!path)
    return absl::nullopt;
  return path->Append(ExecutableFolderPath())
      .AppendASCII(base::StrCat({PRODUCT_FULLNAME_STRING, kExecutableSuffix}));
}

base::FilePath GetExecutableRelativePath() {
  return ExecutableFolderPath().Append(
      base::StrCat({PRODUCT_FULLNAME_STRING, kExecutableSuffix}));
}

absl::optional<base::FilePath> GetKeystoneFolderPath(UpdaterScope scope) {
  absl::optional<base::FilePath> path = GetLibraryFolderPath(scope);
  if (!path)
    return absl::nullopt;
  return path->Append(FILE_PATH_LITERAL(COMPANY_SHORTNAME_STRING))
      .Append(FILE_PATH_LITERAL(KEYSTONE_NAME));
}

}  // namespace updater
