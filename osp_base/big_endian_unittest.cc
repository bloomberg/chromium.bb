// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp_base/big_endian.h"

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace openscreen {
namespace {

// Tests that ReadBigEndian() correctly imports values from various offsets in
// memory.
TEST(BigEndianTest, ReadValues) {
  const uint8_t kInput[] = {
      0,    1,    2,    3,    4,    5,    6,    7,    8,    9,    0xa,
      0xb,  0xc,  0xd,  0xe,  0xf,  0xff, 0xff, 0xfe, 0xff, 0xff, 0xff,
      0xff, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe,
  };

  EXPECT_EQ(UINT16_C(0x0001), ReadBigEndian<uint16_t>(kInput));
  EXPECT_EQ(UINT16_C(0x0102), ReadBigEndian<uint16_t>(kInput + 1));
  EXPECT_EQ(UINT16_C(0x0203), ReadBigEndian<uint16_t>(kInput + 2));
  EXPECT_EQ(-1, ReadBigEndian<int16_t>(kInput + 16));
  EXPECT_EQ(-2, ReadBigEndian<int16_t>(kInput + 17));

  EXPECT_EQ(UINT32_C(0x03040506), ReadBigEndian<uint32_t>(kInput + 3));
  EXPECT_EQ(UINT32_C(0x04050607), ReadBigEndian<uint32_t>(kInput + 4));
  EXPECT_EQ(UINT32_C(0x05060708), ReadBigEndian<uint32_t>(kInput + 5));
  EXPECT_EQ(-1, ReadBigEndian<int32_t>(kInput + 19));
  EXPECT_EQ(-2, ReadBigEndian<int32_t>(kInput + 20));

  EXPECT_EQ(UINT64_C(0x0001020304050607), ReadBigEndian<uint64_t>(kInput));
  EXPECT_EQ(UINT64_C(0x0102030405060708), ReadBigEndian<uint64_t>(kInput + 1));
  EXPECT_EQ(UINT64_C(0x0203040506070809), ReadBigEndian<uint64_t>(kInput + 2));
  EXPECT_EQ(-1, ReadBigEndian<int64_t>(kInput + 24));
  EXPECT_EQ(-2, ReadBigEndian<int64_t>(kInput + 25));
}

// Tests that WriteBigEndian() correctly writes-out values to various offsets in
// memory. This test assumes ReadBigEndian() is working, using it to verify that
// WriteBigEndian() is working.
TEST(BigEndianTest, WriteValues) {
  uint8_t scratch[16];

  WriteBigEndian<uint16_t>(0x0102, scratch);
  EXPECT_EQ(UINT16_C(0x0102), ReadBigEndian<uint16_t>(scratch));
  WriteBigEndian<uint16_t>(0x0304, scratch + 1);
  EXPECT_EQ(UINT16_C(0x0304), ReadBigEndian<uint16_t>(scratch + 1));
  WriteBigEndian<uint16_t>(0x0506, scratch + 2);
  EXPECT_EQ(UINT16_C(0x0506), ReadBigEndian<uint16_t>(scratch + 2));
  WriteBigEndian<int16_t>(42, scratch + 3);
  EXPECT_EQ(42, ReadBigEndian<int16_t>(scratch + 3));
  WriteBigEndian<int16_t>(-1, scratch + 4);
  EXPECT_EQ(-1, ReadBigEndian<int16_t>(scratch + 4));
  WriteBigEndian<int16_t>(-2, scratch + 5);
  EXPECT_EQ(-2, ReadBigEndian<int16_t>(scratch + 5));

  WriteBigEndian<uint32_t>(UINT32_C(0x03040506), scratch);
  EXPECT_EQ(UINT32_C(0x03040506), ReadBigEndian<uint32_t>(scratch));
  WriteBigEndian<uint32_t>(UINT32_C(0x0708090a), scratch + 1);
  EXPECT_EQ(UINT32_C(0x0708090a), ReadBigEndian<uint32_t>(scratch + 1));
  WriteBigEndian<uint32_t>(UINT32_C(0x0b0c0d0e), scratch + 2);
  EXPECT_EQ(UINT32_C(0x0b0c0d0e), ReadBigEndian<uint32_t>(scratch + 2));
  WriteBigEndian<int32_t>(42, scratch + 3);
  EXPECT_EQ(42, ReadBigEndian<int32_t>(scratch + 3));
  WriteBigEndian<int32_t>(-1, scratch + 4);
  EXPECT_EQ(-1, ReadBigEndian<int32_t>(scratch + 4));
  WriteBigEndian<int32_t>(-2, scratch + 5);
  EXPECT_EQ(-2, ReadBigEndian<int32_t>(scratch + 5));

  WriteBigEndian<uint64_t>(UINT64_C(0x0f0e0d0c0b0a0908), scratch);
  EXPECT_EQ(UINT64_C(0x0f0e0d0c0b0a0908), ReadBigEndian<uint64_t>(scratch));
  WriteBigEndian<uint64_t>(UINT64_C(0x0708090a0b0c0d0e), scratch + 1);
  EXPECT_EQ(UINT64_C(0x0708090a0b0c0d0e), ReadBigEndian<uint64_t>(scratch + 1));
  WriteBigEndian<uint64_t>(UINT64_C(0x99aa88bb77cc66dd), scratch + 2);
  EXPECT_EQ(UINT64_C(0x99aa88bb77cc66dd), ReadBigEndian<uint64_t>(scratch + 2));
  WriteBigEndian<int64_t>(42, scratch + 3);
  EXPECT_EQ(42, ReadBigEndian<int64_t>(scratch + 3));
  WriteBigEndian<int64_t>(-1, scratch + 4);
  EXPECT_EQ(-1, ReadBigEndian<int64_t>(scratch + 4));
  WriteBigEndian<int64_t>(-2, scratch + 5);
  EXPECT_EQ(-2, ReadBigEndian<int64_t>(scratch + 5));
}

}  // namespace
}  // namespace openscreen
