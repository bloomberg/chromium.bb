// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/blob/BlobData.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

TEST(BlobDataTest, Consolidation) {
  const size_t kMaxConsolidatedItemSizeInBytes = 15 * 1024;
  BlobData data(BlobData::FileCompositionStatus::NO_UNKNOWN_SIZE_FILES);
  const char* text1 = "abc";
  const char* text2 = "def";
  data.AppendBytes(text1, 3u);
  data.AppendBytes(text2, 3u);
  data.AppendText("ps1", false);
  data.AppendText("ps2", false);

  EXPECT_EQ(1u, data.items_.size());
  EXPECT_EQ(12u, data.items_[0].data->length());
  EXPECT_EQ(0, memcmp(data.items_[0].data->Data(), "abcdefps1ps2", 12));

  std::unique_ptr<char[]> large_data =
      WrapArrayUnique(new char[kMaxConsolidatedItemSizeInBytes]);
  data.AppendBytes(large_data.get(), kMaxConsolidatedItemSizeInBytes);

  EXPECT_EQ(2u, data.items_.size());
  EXPECT_EQ(12u, data.items_[0].data->length());
  EXPECT_EQ(kMaxConsolidatedItemSizeInBytes, data.items_[1].data->length());
}

}  // namespace blink
