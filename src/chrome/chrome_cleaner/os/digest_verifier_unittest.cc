// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/digest_verifier.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "chrome/chrome_cleaner/os/pre_fetched_paths.h"
#include "chrome/chrome_cleaner/os/system_util.h"
#include "chrome/chrome_cleaner/test/resources/grit/test_resources.h"
#include "chrome/chrome_cleaner/test/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

TEST(DigestVerifier, KnownFile) {
  std::shared_ptr<DigestVerifier> digest_verifier =
      DigestVerifier::CreateFromResource(IDS_TEST_SAMPLE_DLL_DIGEST);
  ASSERT_TRUE(digest_verifier);

  base::FilePath dll_path = GetSampleDLLPath();
  ASSERT_TRUE(base::PathExists(dll_path)) << dll_path.value();

  EXPECT_TRUE(digest_verifier->IsKnownFile(dll_path));
}

TEST(DigestVerifier, UnknownFile) {
  std::shared_ptr<DigestVerifier> digest_verifier =
      DigestVerifier::CreateFromResource(IDS_TEST_SAMPLE_DLL_DIGEST);
  ASSERT_TRUE(digest_verifier);

  const base::FilePath self_path =
      PreFetchedPaths::GetInstance()->GetExecutablePath();

  EXPECT_FALSE(digest_verifier->IsKnownFile(self_path));
}

TEST(DigestVerifier, InexistentFile) {
  std::shared_ptr<DigestVerifier> digest_verifier =
      DigestVerifier::CreateFromResource(IDS_TEST_SAMPLE_DLL_DIGEST);
  ASSERT_TRUE(digest_verifier);

  base::FilePath invalid_path(L"this_file_should_not_exist.dll");
  ASSERT_FALSE(base::PathExists(invalid_path));

  EXPECT_FALSE(digest_verifier->IsKnownFile(invalid_path));
}

}  // namespace chrome_cleaner
