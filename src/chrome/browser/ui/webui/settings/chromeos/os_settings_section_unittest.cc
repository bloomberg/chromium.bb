// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/os_settings_section.h"

#include "chrome/browser/ui/webui/settings/chromeos/constants/setting.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace settings {

TEST(OsSettingsSectionTest, Section) {
  // Sections should not incur modification.
  EXPECT_EQ("internet", OsSettingsSection::GetDefaultModifiedUrl(
                            /*type=*/mojom::SearchResultType::kSection,
                            /*id=*/{.section = mojom::Section::kNetwork},
                            /*url_to_modify=*/"internet"));
}

TEST(OsSettingsSectionTest, Subpage) {
  // Subpages should not incur modification.
  EXPECT_EQ("networks?type=WiFi",
            OsSettingsSection::GetDefaultModifiedUrl(
                /*type=*/mojom::SearchResultType::kSubpage,
                /*id=*/{.subpage = mojom::Subpage::kWifiNetworks},
                /*url_to_modify=*/"networks?type=WiFi"));
}

TEST(OsSettingsSectionTest, Setting) {
  // Settings should have settingId added
  EXPECT_EQ("networks?settingId=4",
            OsSettingsSection::GetDefaultModifiedUrl(
                /*type=*/mojom::SearchResultType::kSetting,
                /*id=*/{.setting = mojom::Setting::kWifiOnOff},
                /*url_to_modify=*/"networks"));
}

TEST(OsSettingsSectionTest, SettingExistingQuery) {
  // Settings with existing query parameters should have settingId added.
  EXPECT_EQ("networks?type=WiFi&settingId=4",
            OsSettingsSection::GetDefaultModifiedUrl(
                /*type=*/mojom::SearchResultType::kSetting,
                /*id=*/{.setting = mojom::Setting::kWifiOnOff},
                /*url_to_modify=*/"networks?type=WiFi"));
}

}  // namespace settings
}  // namespace chromeos
