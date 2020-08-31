// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <limits>
#include <memory>

#include "core/fxcodec/basic/basicmodule.h"
#include "core/fxcrt/fx_memory_wrappers.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(fxcodec, A85TestBadInputs) {
  const uint8_t src_buf[] = {1, 2, 3, 4};
  std::unique_ptr<uint8_t, FxFreeDeleter> dest_buf;
  uint32_t dest_size = 0;

  // Error codes, not segvs, should callers pass us a nullptr pointer.
  EXPECT_FALSE(BasicModule::A85Encode(src_buf, &dest_buf, nullptr));
  EXPECT_FALSE(BasicModule::A85Encode(src_buf, nullptr, &dest_size));
  EXPECT_FALSE(BasicModule::A85Encode({}, &dest_buf, &dest_size));
}

// No leftover bytes, just translate 2 sets of symbols.
TEST(fxcodec, A85TestBasic) {
  // Make sure really big values don't break.
  const uint8_t src_buf[] = {1, 2, 3, 4, 255, 255, 255, 255};
  std::unique_ptr<uint8_t, FxFreeDeleter> dest_buf;
  uint32_t dest_size = 0;
  EXPECT_TRUE(BasicModule::A85Encode(src_buf, &dest_buf, &dest_size));

  // Should have 5 chars for each set of 4 and 2 terminators.
  const uint8_t expected_out[] = {33, 60, 78, 63, 43,  115,
                                  56, 87, 45, 33, 126, 62};
  ASSERT_EQ(FX_ArraySize(expected_out), dest_size);

  // Check the output
  auto dest_buf_span = pdfium::make_span(dest_buf.get(), dest_size);
  for (uint32_t i = 0; i < dest_size; i++)
    EXPECT_EQ(expected_out[i], dest_buf_span[i]) << " at " << i;
}

// Leftover bytes.
TEST(fxcodec, A85TestLeftoverBytes) {
  std::unique_ptr<uint8_t, FxFreeDeleter> dest_buf;
  uint32_t dest_size = 0;

  {
    // 1 Leftover Byte:
    const uint8_t src_buf_1leftover[] = {1, 2, 3, 4, 255};
    EXPECT_TRUE(
        BasicModule::A85Encode(src_buf_1leftover, &dest_buf, &dest_size));

    // 5 chars for first symbol + 2 + 2 terminators.
    uint8_t expected_out_1leftover[] = {33, 60, 78, 63, 43, 114, 114, 126, 62};
    ASSERT_EQ(FX_ArraySize(expected_out_1leftover), dest_size);

    // Check the output
    auto dest_buf_span = pdfium::make_span(dest_buf.get(), dest_size);
    for (uint32_t i = 0; i < dest_size; i++)
      EXPECT_EQ(expected_out_1leftover[i], dest_buf_span[i]) << " at " << i;
  }

  {
    // 2 Leftover bytes:
    const uint8_t src_buf_2leftover[] = {1, 2, 3, 4, 255, 254};
    dest_buf.reset();
    dest_size = 0;
    EXPECT_TRUE(
        BasicModule::A85Encode(src_buf_2leftover, &dest_buf, &dest_size));
    // 5 chars for first symbol + 3 + 2 terminators.
    const uint8_t expected_out_2leftover[] = {33,  60, 78, 63,  43,
                                              115, 56, 68, 126, 62};
    ASSERT_EQ(FX_ArraySize(expected_out_2leftover), dest_size);

    // Check the output
    auto dest_buf_span = pdfium::make_span(dest_buf.get(), dest_size);
    for (uint32_t i = 0; i < dest_size; i++)
      EXPECT_EQ(expected_out_2leftover[i], dest_buf_span[i]) << " at " << i;
  }

  {
    // 3 Leftover bytes:
    const uint8_t src_buf_3leftover[] = {1, 2, 3, 4, 255, 254, 253};
    dest_buf.reset();
    dest_size = 0;
    EXPECT_TRUE(
        BasicModule::A85Encode(src_buf_3leftover, &dest_buf, &dest_size));
    // 5 chars for first symbol + 4 + 2 terminators.
    const uint8_t expected_out_3leftover[] = {33, 60, 78,  63,  43, 115,
                                              56, 77, 114, 126, 62};
    ASSERT_EQ(FX_ArraySize(expected_out_3leftover), dest_size);

    // Check the output
    auto dest_buf_span = pdfium::make_span(dest_buf.get(), dest_size);
    for (uint32_t i = 0; i < dest_size; i++)
      EXPECT_EQ(expected_out_3leftover[i], dest_buf_span[i]) << " at " << i;
  }
}

// Test all zeros comes through as "z".
TEST(fxcodec, A85TestZeros) {
  std::unique_ptr<uint8_t, FxFreeDeleter> dest_buf;
  uint32_t dest_size = 0;

  {
    // Make sure really big values don't break.
    const uint8_t src_buf[] = {1, 2, 3, 4, 0, 0, 0, 0};
    EXPECT_TRUE(BasicModule::A85Encode(src_buf, &dest_buf, &dest_size));

    // Should have 5 chars for first set of 4 + 1 for z + 2 terminators.
    const uint8_t expected_out[] = {33, 60, 78, 63, 43, 122, 126, 62};
    ASSERT_EQ(FX_ArraySize(expected_out), dest_size);

    // Check the output
    auto dest_buf_span = pdfium::make_span(dest_buf.get(), dest_size);
    for (uint32_t i = 0; i < dest_size; i++)
      EXPECT_EQ(expected_out[i], dest_buf_span[i]) << " at " << i;
  }

  {
    // Should also work if it is at the start:
    const uint8_t src_buf_2[] = {0, 0, 0, 0, 1, 2, 3, 4};
    dest_buf.reset();
    dest_size = 0;
    EXPECT_TRUE(BasicModule::A85Encode(src_buf_2, &dest_buf, &dest_size));

    // Should have 5 chars for set of 4 + 1 for z + 2 terminators.
    const uint8_t expected_out_2[] = {122, 33, 60, 78, 63, 43, 126, 62};
    ASSERT_EQ(FX_ArraySize(expected_out_2), dest_size);

    // Check the output
    auto dest_buf_span = pdfium::make_span(dest_buf.get(), dest_size);
    for (uint32_t i = 0; i < dest_size; i++)
      EXPECT_EQ(expected_out_2[i], dest_buf_span[i]) << " at " << i;
  }

  {
    // Try with 2 leftover zero bytes. Make sure we don't get a "z".
    const uint8_t src_buf_3[] = {1, 2, 3, 4, 0, 0};
    dest_buf.reset();
    dest_size = 0;
    EXPECT_TRUE(BasicModule::A85Encode(src_buf_3, &dest_buf, &dest_size));

    // Should have 5 chars for set of 4 + 3 for last 2 + 2 terminators.
    const uint8_t expected_out_leftover[] = {33, 60, 78, 63,  43,
                                             33, 33, 33, 126, 62};
    ASSERT_EQ(FX_ArraySize(expected_out_leftover), dest_size);

    // Check the output
    auto dest_buf_span = pdfium::make_span(dest_buf.get(), dest_size);
    for (uint32_t i = 0; i < dest_size; i++)
      EXPECT_EQ(expected_out_leftover[i], dest_buf_span[i]) << " at " << i;
  }
}

// Make sure we get returns in the expected locations.
TEST(fxcodec, A85TestLineBreaks) {
  // Make sure really big values don't break.
  uint8_t src_buf[131] = {0};
  // 1 full line + most of a line of normal symbols.
  for (int k = 0; k < 116; k += 4) {
    src_buf[k] = 1;
    src_buf[k + 1] = 2;
    src_buf[k + 2] = 3;
    src_buf[k + 3] = 4;
  }
  // Fill in the end, leaving an all zero gap + 3 extra zeros at the end.
  for (int k = 120; k < 128; k++) {
    src_buf[k] = 1;
    src_buf[k + 1] = 2;
    src_buf[k + 2] = 3;
    src_buf[k + 3] = 4;
  }
  std::unique_ptr<uint8_t, FxFreeDeleter> dest_buf;
  uint32_t dest_size = 0;

  // Should succeed.
  EXPECT_TRUE(BasicModule::A85Encode(src_buf, &dest_buf, &dest_size));

  // Should have 75 chars in the first row plus 2 char return,
  // 76 chars in the second row plus 2 char return,
  // and 9 chars in the last row with 2 terminators.
  ASSERT_EQ(166u, dest_size);

  // Check for the returns.
  auto dest_buf_span = pdfium::make_span(dest_buf.get(), dest_size);
  EXPECT_EQ(13, dest_buf_span[75]);
  EXPECT_EQ(10, dest_buf_span[76]);
  EXPECT_EQ(13, dest_buf_span[153]);
  EXPECT_EQ(10, dest_buf_span[154]);
}
