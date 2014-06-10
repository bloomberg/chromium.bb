// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "leb128.h"

#include <vector>
#include "testing/gtest/include/gtest/gtest.h"

namespace relocation_packer {

TEST(Leb128, Encoder) {
  std::vector<uint32_t> values;
  values.push_back(624485u);
  values.push_back(0u);
  values.push_back(1u);
  values.push_back(127u);
  values.push_back(128u);

  Leb128Encoder encoder;
  encoder.EnqueueAll(values);

  std::vector<uint8_t> encoding;
  encoder.GetEncoding(&encoding);

  EXPECT_EQ(8u, encoding.size());
  // 624485
  EXPECT_EQ(0xe5, encoding[0]);
  EXPECT_EQ(0x8e, encoding[1]);
  EXPECT_EQ(0x26, encoding[2]);
  // 0
  EXPECT_EQ(0x00, encoding[3]);
  // 1
  EXPECT_EQ(0x01, encoding[4]);
  // 127
  EXPECT_EQ(0x7f, encoding[5]);
  // 128
  EXPECT_EQ(0x80, encoding[6]);
  EXPECT_EQ(0x01, encoding[7]);

  encoder.Enqueue(4294967295u);

  encoding.clear();
  encoder.GetEncoding(&encoding);

  EXPECT_EQ(13u, encoding.size());
  // 4294967295
  EXPECT_EQ(0xff, encoding[8]);
  EXPECT_EQ(0xff, encoding[9]);
  EXPECT_EQ(0xff, encoding[10]);
  EXPECT_EQ(0xff, encoding[11]);
  EXPECT_EQ(0x0f, encoding[12]);
}

TEST(Leb128, Decoder) {
  std::vector<uint8_t> encoding;
  // 624485
  encoding.push_back(0xe5);
  encoding.push_back(0x8e);
  encoding.push_back(0x26);
  // 0
  encoding.push_back(0x00);
  // 1
  encoding.push_back(0x01);
  // 127
  encoding.push_back(0x7f);
  // 128
  encoding.push_back(0x80);
  encoding.push_back(0x01);
  // 4294967295
  encoding.push_back(0xff);
  encoding.push_back(0xff);
  encoding.push_back(0xff);
  encoding.push_back(0xff);
  encoding.push_back(0x0f);

  Leb128Decoder decoder(encoding);

  EXPECT_EQ(624485u, decoder.Dequeue());

  std::vector<uint32_t> dequeued;
  decoder.DequeueAll(&dequeued);

  EXPECT_EQ(5u, dequeued.size());
  EXPECT_EQ(0u, dequeued[0]);
  EXPECT_EQ(1u, dequeued[1]);
  EXPECT_EQ(127u, dequeued[2]);
  EXPECT_EQ(128u, dequeued[3]);
  EXPECT_EQ(4294967295u, dequeued[4]);
}

}  // namespace relocation_packer
