// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/util/data_encryption_win.h"

#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

using std::string;
using std::vector;

namespace {

TEST(SyncDataEncryption, TestEncryptDecryptOfSampleString) {
  vector<uint8> example(EncryptData("example"));
  ASSERT_FALSE(example.empty());
  string result;
  ASSERT_TRUE(DecryptData(example, &result));
  ASSERT_TRUE(result == "example");
}

TEST(SyncDataEncryption, TestDecryptFailure) {
  vector<uint8> example(0, 0);
  string result;
  ASSERT_FALSE(DecryptData(example, &result));
}

}  // namespace
