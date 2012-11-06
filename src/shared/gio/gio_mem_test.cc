/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include "native_client/src/shared/gio/gio.h"
#include "native_client/src/shared/gio/gio_test_base.h"
#include "gtest/gtest.h"

namespace {

TEST(GioMemTest, ReadTest) {
  struct GioMemoryFile mf;
  char* in_buffer;
  int in_size = expected_file_size;
  int ret_code;

  in_buffer = reinterpret_cast<char*>(malloc(in_size));
  GioInitTestMemFile(in_buffer, 'A', in_size);
  ret_code = GioMemoryFileCtor(&mf, in_buffer, in_size);
  EXPECT_EQ(1, ret_code);

  GioReadTestWithOffset(&mf.base, 'A');
  GioCloseTest(&mf.base);
  free(in_buffer);
}

TEST(GioMemTest, WriteTest) {
  struct GioMemoryFile mf;
  char* mf_buffer;
  int mf_size = 64;
  int ret_code;

  mf_buffer = reinterpret_cast<char*>(malloc(mf_size));
  EXPECT_NE(reinterpret_cast<char*>(NULL), mf_buffer);

  ret_code = GioMemoryFileCtor(&mf, mf_buffer, mf_size);
  EXPECT_EQ(1, ret_code);
  GioWriteTest(&mf.base, true);
  GioCloseTest(&mf.base);
  free(mf_buffer);
}

TEST(GioMemTest, SeekTest) {
  struct GioMemoryFile mf;
  char* in_buffer;
  int in_size = expected_file_size;
  int ret_code;

  in_buffer = reinterpret_cast<char*>(malloc(in_size));
  GioInitTestMemFile(in_buffer, 0, in_size);
  ret_code = GioMemoryFileCtor(&mf, in_buffer, in_size);
  EXPECT_EQ(1, ret_code);

  GioSeekTestWithOffset(&mf.base, 0, true);
  GioCloseTest(&mf.base);
  free(in_buffer);
}

}  // namespace

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
