// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector/get_installed_version.h"

#include <stdint.h>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/install_util.h"
#include "components/version_info/version_info.h"
#include "testing/gtest/include/gtest/gtest.h"

class GetInstalledVersionWinTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        registry_overrides_.OverrideRegistry(HKEY_CURRENT_USER));
  }

  void SetInstalledVersion(const base::Version& version) {
    base::win::RegKey key(HKEY_CURRENT_USER,
                          install_static::GetClientsKeyPath().c_str(),
                          KEY_SET_VALUE);
    ASSERT_TRUE(key.Valid());
    ASSERT_EQ(key.WriteValue(google_update::kRegVersionField,
                             base::ASCIIToUTF16(version.GetString()).c_str()),
              ERROR_SUCCESS);
  }

  void SetCriticalVersion(const base::Version& version) {
    base::win::RegKey key(HKEY_CURRENT_USER,
                          install_static::GetClientsKeyPath().c_str(),
                          KEY_SET_VALUE);
    ASSERT_TRUE(key.Valid());
    ASSERT_EQ(key.WriteValue(google_update::kRegCriticalVersionField,
                             base::ASCIIToUTF16(version.GetString()).c_str()),
              ERROR_SUCCESS);
  }

 private:
  registry_util::RegistryOverrideManager registry_overrides_;
};

// Tests that an empty instance is returned when no version is in the registry.
TEST_F(GetInstalledVersionWinTest, NoVersion) {
  InstalledAndCriticalVersion versions = GetInstalledVersion();
  EXPECT_FALSE(versions.installed_version.IsValid());
  EXPECT_FALSE(versions.critical_version.has_value());
}

// Tests that the product version in the registry is returned.
TEST_F(GetInstalledVersionWinTest, WithVersion) {
  ASSERT_NO_FATAL_FAILURE(SetInstalledVersion(version_info::GetVersion()));
  InstalledAndCriticalVersion versions = GetInstalledVersion();
  ASSERT_TRUE(versions.installed_version.IsValid());
  EXPECT_EQ(versions.installed_version, version_info::GetVersion());
  EXPECT_FALSE(versions.critical_version.has_value());
}

// Tests that the product version and critical version in the registry are
// returned.
TEST_F(GetInstalledVersionWinTest, WithCriticalVersion) {
  std::vector<uint32_t> components = version_info::GetVersion().components();
  --components[0];
  base::Version critical_version = base::Version(std::move(components));
  ASSERT_NO_FATAL_FAILURE(SetInstalledVersion(version_info::GetVersion()));
  ASSERT_NO_FATAL_FAILURE(SetCriticalVersion(critical_version));
  InstalledAndCriticalVersion versions = GetInstalledVersion();
  ASSERT_TRUE(versions.installed_version.IsValid());
  EXPECT_EQ(versions.installed_version, version_info::GetVersion());
  ASSERT_TRUE(versions.critical_version.has_value());
  ASSERT_TRUE(versions.critical_version.value().IsValid());
  EXPECT_EQ(versions.critical_version.value(), critical_version);
}
