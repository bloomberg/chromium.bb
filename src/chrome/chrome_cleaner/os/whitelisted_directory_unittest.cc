// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/whitelisted_directory.h"

#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/test/scoped_path_override.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

TEST(WhitelistedDirectory, IsWhitelisted) {
  base::FilePath temp_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_TEMP, &temp_dir));

  EXPECT_TRUE(
      WhitelistedDirectory::GetInstance()->IsWhitelistedDirectory(temp_dir));
}

TEST(WhitelistedDirectory, NotWhitelisted) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());

  EXPECT_FALSE(WhitelistedDirectory::GetInstance()->IsWhitelistedDirectory(
      scoped_temp_dir.GetPath()));
}

// Ensure that the test environment isn't caching whitelisted directories, since
// they can be overridden by various tests.
TEST(WhitelistedDirectory, TestsNotCached) {
  base::FilePath original_temp_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_TEMP, &original_temp_dir));
  EXPECT_TRUE(WhitelistedDirectory::GetInstance()->IsWhitelistedDirectory(
      original_temp_dir));

  base::ScopedPathOverride temp_override(base::DIR_TEMP);
  base::FilePath scoped_temp_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_TEMP, &scoped_temp_dir));

  EXPECT_FALSE(WhitelistedDirectory::GetInstance()->IsWhitelistedDirectory(
      original_temp_dir));

  EXPECT_TRUE(WhitelistedDirectory::GetInstance()->IsWhitelistedDirectory(
      scoped_temp_dir));
}

}  // namespace chrome_cleaner
