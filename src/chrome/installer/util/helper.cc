// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/helper.h"

#include "base/path_service.h"
#include "base/win/registry.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/initial_preferences.h"
#include "chrome/installer/util/initial_preferences_constants.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/util_constants.h"

namespace {

// Returns a valid file path with the proper casing from the system if
// |target_dir| is a valid target for the browser's installation and
// |system_install| is true. Returns an empty file path otherwise.
base::FilePath GetInstallationDirFromPrefs(
    const installer::InitialPreferences& prefs,
    bool system_install) {
  base::FilePath program_files_dir;
  if (system_install) {
    prefs.GetPath(installer::initial_preferences::kProgramFilesDir,
                  &program_files_dir);
  }
  if (program_files_dir.empty())
    return program_files_dir;

  base::FilePath expected_dir;
  bool valid_program_files_path =
      ((base::PathService::Get(base::DIR_PROGRAM_FILES, &expected_dir) &&
        base::FilePath::CompareEqualIgnoreCase(program_files_dir.value(),
                                               expected_dir.value())) ||
       (base::PathService::Get(base::DIR_PROGRAM_FILESX86, &expected_dir) &&
        base::FilePath::CompareEqualIgnoreCase(program_files_dir.value(),
                                               expected_dir.value())));

  return valid_program_files_path
             ? expected_dir
                   .Append(install_static::GetChromeInstallSubDirectory())
                   .Append(installer::kInstallBinaryDir)
             : base::FilePath();
}

base::FilePath GetDefaultChromeInstallPath(bool system_install) {
  base::FilePath install_path;
  int key = system_install ? base::DIR_PROGRAM_FILES : base::DIR_LOCAL_APP_DATA;
  if (base::PathService::Get(key, &install_path)) {
    install_path =
        install_path.Append(install_static::GetChromeInstallSubDirectory());
    install_path = install_path.Append(installer::kInstallBinaryDir);
  }
  return install_path;
}

base::FilePath GetCurrentInstallPathFromRegistry(bool system_install) {
  base::FilePath install_path;
  if (!InstallUtil::GetChromeVersion(system_install).IsValid())
    return install_path;

  std::wstring uninstall_string;
  base::win::RegKey key(system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
                        install_static::GetClientStateKeyPath().c_str(),
                        KEY_QUERY_VALUE | KEY_WOW64_32KEY);
  key.ReadValue(installer::kUninstallStringField, &uninstall_string);

  // The UninstallString has the format
  // [InstallPath]/[version]/Installer/setup.exe. In order to get the
  // [InstallPath], the full path must be pruned of the last 3 components.
  if (!uninstall_string.empty()) {
    install_path = base::FilePath(std::move(uninstall_string))
                       .DirName()
                       .DirName()
                       .DirName();
  }
  return install_path;
}

}  // namespace

namespace installer {

base::FilePath GetChromeInstallPath(bool system_install) {
  base::FilePath install_path =
      GetCurrentInstallPathFromRegistry(system_install);
  if (install_path.empty())
    install_path = GetDefaultChromeInstallPath(system_install);
  return install_path;
}

base::FilePath GetChromeInstallPathWithPrefs(bool system_install,
                                             const InitialPreferences& prefs) {
  base::FilePath install_path =
      GetCurrentInstallPathFromRegistry(system_install);
  if (!install_path.empty())
    return install_path;

  install_path = GetInstallationDirFromPrefs(prefs, system_install);
  if (install_path.empty())
    install_path = GetDefaultChromeInstallPath(system_install);
  return install_path;
}

}  // namespace installer.
