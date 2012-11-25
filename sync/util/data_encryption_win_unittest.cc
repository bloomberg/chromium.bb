// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/util/data_encryption_win.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

TEST(SyncDataEncryption, TestEncryptDecryptOfSampleString) {
  std::vector<uint8> example(EncryptData("example"));
  ASSERT_FALSE(example.empty());
  std::string result;
  ASSERT_TRUE(DecryptData(example, &result));
  ASSERT_TRUE(result == "example");
}

TEST(SyncDataEncryption, TestDecryptFailure) {
  std::vector<uint8> example(0, 0);
  std::string result;
  ASSERT_FALSE(DecryptData(example, &result));
}

}  // namespace
}  // namespace syncer
