// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/syncable/entry_kernel.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace syncable {

class EntryKernelTest : public testing::Test {};

TEST_F(EntryKernelTest, ToValue) {
  EntryKernel kernel;
  scoped_ptr<base::DictionaryValue> value(kernel.ToValue(NULL));
  if (value) {
    // Not much to check without repeating the ToValue() code.
    EXPECT_TRUE(value->HasKey("isDirty"));
    // The extra +2 is for "isDirty" and "serverModelType".
    EXPECT_EQ(BIT_TEMPS_END - BEGIN_FIELDS + 2,
              static_cast<int>(value->size()));
  } else {
    ADD_FAILURE();
  }
}

}  // namespace syncable

}  // namespace syncer
