// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/soda_ja_jp_component_installer.h"

#include "base/files/file_path.h"
#include "base/test/bind.h"
#include "base/version.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

class SodaJaJpComponentInstallerTest : public ::testing::Test {
 public:
  SodaJaJpComponentInstallerTest()
      : fake_install_dir_(FILE_PATH_LITERAL("base/install/dir/")),
        fake_version_("0.0.1") {}

 protected:
  base::FilePath fake_install_dir_;
  base::Version fake_version_;
};

TEST_F(SodaJaJpComponentInstallerTest, ComponentReady_CallsLambda) {
  base::FilePath given_path;
  OnSodaJaJpComponentReadyCallback lambda = base::BindLambdaForTesting(
      [&](const base::FilePath& path) { given_path = path; });

  SodaJaJpComponentInstallerPolicy policy(std::move(lambda));

  policy.ComponentReady(fake_version_, fake_install_dir_,
                        std::make_unique<base::DictionaryValue>());

  ASSERT_EQ(fake_install_dir_, given_path);
}

}  // namespace component_updater
