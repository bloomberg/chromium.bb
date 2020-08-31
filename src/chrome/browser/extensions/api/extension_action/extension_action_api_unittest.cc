// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/common/extensions/api/extension_action/action_info_test_util.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {
namespace {

class ExtensionActionAPIUnitTest
    : public ExtensionServiceTestWithInstall,
      public ::testing::WithParamInterface<ActionInfo::Type> {
 public:
  ExtensionActionAPIUnitTest()
      : current_channel_(GetOverrideChannelForActionType(GetParam())) {}
  ~ExtensionActionAPIUnitTest() override {}

 private:
  std::unique_ptr<ScopedCurrentChannel> current_channel_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionActionAPIUnitTest);
};

// Test that extensions can provide icons of arbitrary sizes in the manifest.
TEST_P(ExtensionActionAPIUnitTest, MultiIcons) {
  InitializeEmptyExtensionService();

  constexpr char kManifestTemplate[] =
      R"({
           "name": "A test extension that tests multiple browser action icons",
           "version": "1.0",
           "manifest_version": 2,
           "%s": {
             "default_icon": {
               "19": "icon19.png",
               "24": "icon24.png",
               "31": "icon24.png",
               "38": "icon38.png"
             }
           }
         })";

  TestExtensionDir test_extension_dir;
  test_extension_dir.WriteManifest(base::StringPrintf(
      kManifestTemplate, GetManifestKeyForActionType(GetParam())));

  {
    std::string icon_file_content;
    base::FilePath icon_path = data_dir().AppendASCII("icon1.png");
    EXPECT_TRUE(base::ReadFileToString(icon_path, &icon_file_content));
    test_extension_dir.WriteFile(FILE_PATH_LITERAL("icon19.png"),
                                 icon_file_content);
    test_extension_dir.WriteFile(FILE_PATH_LITERAL("icon24.png"),
                                 icon_file_content);
    test_extension_dir.WriteFile(FILE_PATH_LITERAL("icon38.png"),
                                 icon_file_content);
  }

  const Extension* extension =
      PackAndInstallCRX(test_extension_dir.UnpackedPath(), INSTALL_NEW);
  EXPECT_TRUE(extension->install_warnings().empty());
  const ActionInfo* action_info = GetActionInfoOfType(*extension, GetParam());
  ASSERT_TRUE(action_info);

  const ExtensionIconSet& icons = action_info->default_icon;

  EXPECT_EQ(4u, icons.map().size());
  EXPECT_EQ("icon19.png", icons.Get(19, ExtensionIconSet::MATCH_EXACTLY));
  EXPECT_EQ("icon24.png", icons.Get(24, ExtensionIconSet::MATCH_EXACTLY));
  EXPECT_EQ("icon24.png", icons.Get(31, ExtensionIconSet::MATCH_EXACTLY));
  EXPECT_EQ("icon38.png", icons.Get(38, ExtensionIconSet::MATCH_EXACTLY));
}

INSTANTIATE_TEST_SUITE_P(All,
                         ExtensionActionAPIUnitTest,
                         testing::Values(ActionInfo::TYPE_BROWSER,
                                         ActionInfo::TYPE_PAGE,
                                         ActionInfo::TYPE_ACTION));

}  // namespace
}  // namespace extensions
