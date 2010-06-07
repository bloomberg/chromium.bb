// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/data_buffer.h"
#include "remoting/host/differ_block.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace remoting {

static const int kWidth = 32;
static const int kHeight = 32;
static const int kBytesPerPixel = 3;

static void GenerateData(uint8* data, int size) {
  for (int i = 0; i < size; ++i) {
    data[i] = i;
  }
}

class EncodeDoneHandler
    : public base::RefCountedThreadSafe<EncodeDoneHandler> {
 public:
  MOCK_METHOD0(EncodeDone, void());
};

TEST(BlockDifferenceTest, BlockDifference) {
  // Prepare 2 blocks to compare.
  uint8 block1[kHeight * kWidth * kBytesPerPixel];
  uint8 block2[kHeight * kWidth * kBytesPerPixel];
  GenerateData(block1, sizeof(block1));
  memcpy(block2, block1, sizeof(block2));

  // These blocks should match.
  int same = BlockDifference(block1, block2, kWidth * kBytesPerPixel);
  EXPECT_EQ(0, same);

  // Change block2 a little.
  block2[7] += 3;
  block2[sizeof(block2)-1] -= 5;
  // These blocks should not match.  The difference should be 8.
  int not_same = BlockDifference(block1, block2, kWidth * kBytesPerPixel);
  EXPECT_EQ(8, not_same);
}

}  // namespace remoting
