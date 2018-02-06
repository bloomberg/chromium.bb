/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#include <stdint.h>

#include "aom/aom_integer.h"
#include "gtest/gtest.h"

namespace {
const uint32_t kSizeTestNumValues = 6;
const uint32_t kSizeTestExpectedSizes[kSizeTestNumValues] = {
  1, 1, 2, 3, 4, 5
};
const uint64_t kSizeTestInputs[kSizeTestNumValues] = {
  0, 0x7f, 0x3fff, 0x1fffff, 0xffffff, 0x10000000
};
}  // namespace

TEST(AomLeb128, DecodeTest) {
  const size_t num_leb128_bytes = 3;
  const uint8_t leb128_bytes[num_leb128_bytes] = { 0xE5, 0x8E, 0x26 };
  const uint64_t expected_value = 0x98765;  // 624485
  uint64_t value = 0;
  aom_uleb_decode(&leb128_bytes[0], num_leb128_bytes, &value);
  ASSERT_EQ(expected_value, value);

  // Make sure the decoder stops on the last marked LEB128 byte.
  aom_uleb_decode(&leb128_bytes[0], num_leb128_bytes + 1, &value);
  ASSERT_EQ(expected_value, value);
}

TEST(AomLeb128, EncodeTest) {
  const uint32_t test_value = 0x98765;  // 624485
  const uint32_t expected_value = 0x00E58E26;
  const size_t kWriteBufferSize = 4;
  uint8_t write_buffer[kWriteBufferSize] = { 0 };
  size_t bytes_written = 0;
  ASSERT_EQ(aom_uleb_encode(test_value, kWriteBufferSize, &write_buffer[0],
                            &bytes_written),
            0);
  ASSERT_EQ(bytes_written, 3u);
  uint32_t encoded_value = 0;
  for (size_t i = 0; i < bytes_written; ++i) {
    const int shift = (int)(bytes_written - 1 - i) * 8;
    encoded_value |= write_buffer[i] << shift;
  }
  ASSERT_EQ(expected_value, encoded_value);
}

TEST(AomLeb128, EncDecTest) {
  const uint32_t value = 0x98765;  // 624485
  const size_t kWriteBufferSize = 4;
  uint8_t write_buffer[kWriteBufferSize] = { 0 };
  size_t bytes_written = 0;
  ASSERT_EQ(aom_uleb_encode(value, kWriteBufferSize, &write_buffer[0],
                            &bytes_written),
            0);
  ASSERT_EQ(bytes_written, 3u);
  uint64_t decoded_value = 0;
  aom_uleb_decode(&write_buffer[0], bytes_written, &decoded_value);
  ASSERT_EQ(value, decoded_value);
}

TEST(AomLeb128, FixedSizeEncodeTest) {
  const uint32_t test_value = 0x0;
  const uint32_t expected_value = 0x00808080;
  const size_t kWriteBufferSize = 4;
  uint8_t write_buffer[kWriteBufferSize] = { 0 };
  size_t bytes_written = 0;
  ASSERT_EQ(0, aom_uleb_encode_fixed_size(test_value, kWriteBufferSize,
                                          kWriteBufferSize, &write_buffer[0],
                                          &bytes_written));
  ASSERT_EQ(kWriteBufferSize, bytes_written);
  uint32_t encoded_value = 0;
  for (size_t i = 0; i < bytes_written; ++i) {
    const int shift = (int)(bytes_written - 1 - i) * 8;
    encoded_value |= write_buffer[i] << shift;
  }
  ASSERT_EQ(expected_value, encoded_value);
}

TEST(AomLeb128, FixedSizeEncDecTest) {
  const uint32_t value = 0x1;
  const size_t kWriteBufferSize = 4;
  uint8_t write_buffer[kWriteBufferSize] = { 0 };
  size_t bytes_written = 0;
  ASSERT_EQ(
      aom_uleb_encode_fixed_size(value, kWriteBufferSize, kWriteBufferSize,
                                 &write_buffer[0], &bytes_written),
      0);
  ASSERT_EQ(bytes_written, 4u);
  uint64_t decoded_value = 0;
  aom_uleb_decode(&write_buffer[0], bytes_written, &decoded_value);
  ASSERT_EQ(value, decoded_value);
}

TEST(AomLeb128, SizeTest) {
  for (size_t i = 0; i < kSizeTestNumValues; ++i) {
    ASSERT_EQ(kSizeTestExpectedSizes[i],
              aom_uleb_size_in_bytes(kSizeTestInputs[i]));
  }
}

TEST(AomLeb128, FailTest) {
  const size_t kWriteBufferSize = 4;
  const uint32_t kValidTestValue = 1;
  uint8_t write_buffer[kWriteBufferSize] = { 0 };
  size_t coded_size = 0;
  ASSERT_EQ(
      aom_uleb_encode(kValidTestValue, kWriteBufferSize, NULL, &coded_size),
      -1);
  ASSERT_EQ(aom_uleb_encode(kValidTestValue, kWriteBufferSize, &write_buffer[0],
                            NULL),
            -1);

  const uint32_t kValueOutOfRange = 0xFFFFFFFF;
  ASSERT_EQ(aom_uleb_encode(kValueOutOfRange, kWriteBufferSize,
                            &write_buffer[0], &coded_size),
            -1);

  const size_t kPadSizeOutOfRange = 5;
  ASSERT_EQ(aom_uleb_encode_fixed_size(kValidTestValue, kWriteBufferSize,
                                       kPadSizeOutOfRange, &write_buffer[0],
                                       &coded_size),
            -1);
}
