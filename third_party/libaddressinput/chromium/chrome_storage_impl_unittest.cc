// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/libaddressinput/chromium/chrome_storage_impl.h"

#include <string>

#include "base/prefs/value_map_pref_store.h"
#include "cpp/test/storage_test_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libaddressinput/chromium/cpp/include/libaddressinput/callback.h"

namespace autofill {

// Tests for ChromeStorageImpl object.
class ChromeStorageImplTest : public testing::Test {
 protected:
  ChromeStorageImplTest()
      : store_(new ValueMapPrefStore()),
        storage_(store_.get()),
        runner_(&storage_) {}

  virtual ~ChromeStorageImplTest() {}

  scoped_refptr<ValueMapPrefStore> store_;
  ChromeStorageImpl storage_;
  i18n::addressinput::StorageTestRunner runner_;
};

TEST_F(ChromeStorageImplTest, StandardStorageTests) {
  EXPECT_NO_FATAL_FAILURE(runner_.RunAllTests());
}

}  // namespace autofill
