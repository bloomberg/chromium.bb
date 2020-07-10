// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/constants/dbus_paths.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/system/sys_info.h"

namespace chromeos {
namespace dbus_paths {

namespace {

constexpr base::FilePath::CharType kDefaultUserPolicyKeysDir[] =
    FILE_PATH_LITERAL("/run/user_policy");

constexpr base::FilePath::CharType kOwnerKeyFileName[] =
    FILE_PATH_LITERAL("/var/lib/whitelist/owner.key");

constexpr base::FilePath::CharType kInstallAttributesFileName[] =
    FILE_PATH_LITERAL("/run/lockbox/install_attributes.pb");

bool PathProvider(int key, base::FilePath* result) {
  switch (key) {
    case DIR_USER_POLICY_KEYS:
      *result = base::FilePath(kDefaultUserPolicyKeysDir);
      break;
    case FILE_OWNER_KEY:
      *result = base::FilePath(kOwnerKeyFileName);
      break;
    case FILE_INSTALL_ATTRIBUTES:
      *result = base::FilePath(kInstallAttributesFileName);
      break;
    default:
      return false;
  }
  return true;
}

}  // namespace

void RegisterPathProvider() {
  base::PathService::RegisterProvider(PathProvider, PATH_START, PATH_END);
}

void RegisterStubPathOverrides(const base::FilePath& stubs_dir) {
  CHECK(!base::SysInfo::IsRunningOnChromeOS());

  // Override these paths when running on Linux, so that enrollment and cloud
  // policy work correctly and can be tested.
  base::FilePath parent = base::MakeAbsoluteFilePath(stubs_dir);
  base::PathService::Override(DIR_USER_POLICY_KEYS,
                              parent.AppendASCII("stub_user_policy"));
  const bool is_absolute = true;
  const bool create = false;
  base::PathService::OverrideAndCreateIfNeeded(
      FILE_OWNER_KEY, parent.AppendASCII("stub_owner.key"), is_absolute,
      create);
  base::PathService::OverrideAndCreateIfNeeded(
      FILE_INSTALL_ATTRIBUTES, parent.AppendASCII("stub_install_attributes.pb"),
      is_absolute, create);
}

}  // namespace dbus_paths
}  // namespace chromeos
