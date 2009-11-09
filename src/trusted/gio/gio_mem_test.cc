/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "native_client/src/trusted/gio/gio.h"
#include "gtest/gtest.h"

namespace {

TEST(GioMemTest, ReadTest) {
  struct GioMemoryFile mf;
  char *in_buffer;
  int in_size = 32;
  char *out_buffer;
  int out_size = 16;
  int ret_code;

  in_buffer = (char*) malloc(in_size);
  for (int i = 0; i < in_size; ++i)
    in_buffer[i]=i+'A';

  GioMemoryFileCtor(&mf, in_buffer, in_size);

  out_buffer = (char*) malloc(out_size);

  // mf_curpos = 0, 32 left, read 16
  ret_code = GioMemoryFileRead(&mf.base, out_buffer, 16);
  EXPECT_EQ(16, ret_code);
  for (int i = 0; i < 16; ++i)
    EXPECT_EQ(i+'A', out_buffer[i]);

  // mf_curpos = 16, 16 left, read 10
  ret_code = GioMemoryFileRead(&mf.base, out_buffer, 10);
  EXPECT_EQ(10, ret_code);
  for (int i = 0; i < 10; ++i)
    EXPECT_EQ(16+i+'A', out_buffer[i]);
  // residual value after idx 10
  EXPECT_EQ(10+'A', out_buffer[10]);

  // mf_curpos = 26, 6 left, read 8
  ret_code = GioMemoryFileRead(&mf.base, out_buffer, 8);
  EXPECT_EQ(6, ret_code);
  for (int i = 0; i < 6; ++i)
    EXPECT_EQ(26+i+'A', out_buffer[i]);
  // residual value after idx 6
  EXPECT_EQ(22+'A', out_buffer[6]);

  // mf_curpos = 32, 0 left, read 16
  ret_code = GioMemoryFileRead(&mf.base, out_buffer, 16);
  EXPECT_EQ(0, ret_code);
}

TEST(GioMemTest, WriteTest) {
  struct GioMemoryFile mf;
  char *mf_buffer;
  int mf_size = 64;
  char *in_buffer;
  int in_size = 32;
  char out_char;
  int ret_code;

  mf_buffer = (char *) malloc(mf_size);
  EXPECT_NE((void*)NULL, mf_buffer);

  GioMemoryFileCtor(&mf, mf_buffer, mf_size);

  in_buffer = (char*) malloc(in_size);
  for (int i = 0; i < in_size; ++i)
    in_buffer[i] = i;

  // mf_curpos = 0, 64 left, write 32
  ret_code = GioMemoryFileWrite(&mf.base, in_buffer, 32);
  EXPECT_EQ(32, ret_code);

  GioMemoryFileSeek(&mf.base, -1, SEEK_CUR);
  GioMemoryFileRead(&mf.base, &out_char, 1);
  EXPECT_EQ(31, out_char);

  // mf_curpos = 32, 32 left, write 20
  ret_code = GioMemoryFileWrite(&mf.base, in_buffer, 20);
  EXPECT_EQ(20, ret_code);

  GioMemoryFileSeek(&mf.base, -1, SEEK_CUR);
  GioMemoryFileRead(&mf.base, &out_char, 1);
  EXPECT_EQ(19, out_char);

  // mf_curpos = 52, 12 left, write 20
  ret_code = GioMemoryFileWrite(&mf.base, in_buffer, 20);
  EXPECT_EQ(12, ret_code);

  GioMemoryFileSeek(&mf.base, -1, SEEK_CUR);
  GioMemoryFileRead(&mf.base, &out_char, 1);
  EXPECT_EQ(11, out_char);

  // mf_curpos = 64, 0 left, write 20
  ret_code = GioMemoryFileWrite(&mf.base, in_buffer, 20);
  EXPECT_EQ(0, ret_code);
}

TEST(GioMemTest, SeekTest) {
  struct GioMemoryFile mf;
  char *in_buffer;
  int in_size = 32;
  char out_char;
  int ret_code;

  in_buffer = (char*) malloc(in_size);
  for (int i = 0; i < in_size; ++i)
    in_buffer[i] = i;

  GioMemoryFileCtor(&mf, in_buffer, in_size);

  // mf_curpos = 0
  ret_code = GioMemoryFileSeek(&mf.base, 15, SEEK_SET);
  EXPECT_EQ(15, ret_code);

  ret_code = GioMemoryFileRead(&mf.base, &out_char, 1);
  EXPECT_EQ(1, ret_code);
  EXPECT_EQ(15, out_char);

  // mf_curpos = 16
  ret_code = GioMemoryFileSeek(&mf.base, 4, SEEK_CUR);
  EXPECT_EQ(20, ret_code);

  ret_code = GioMemoryFileRead(&mf.base, &out_char, 1);
  EXPECT_EQ(1, ret_code);
  EXPECT_EQ(20, out_char);

  // mf_curpos = 21
  ret_code = GioMemoryFileSeek(&mf.base, -4, SEEK_CUR);
  EXPECT_EQ(17, ret_code);

  ret_code = GioMemoryFileRead(&mf.base, &out_char, 1);
  EXPECT_EQ(1, ret_code);
  EXPECT_EQ(17, out_char);

  // mf_curpos = 17
  ret_code = GioMemoryFileSeek(&mf.base, -4, SEEK_END);
  EXPECT_EQ(28, ret_code);

  GioMemoryFileRead(&mf.base, &out_char, 1);
  EXPECT_EQ(28, out_char);

  // mf_curpos = 29
  ret_code = GioMemoryFileSeek(&mf.base, 4, SEEK_END);
  EXPECT_EQ(-1, ret_code);

  GioMemoryFileRead(&mf.base, &out_char, 1);
  EXPECT_EQ(29, out_char);

  // mf_curpos = 30
  ret_code = GioMemoryFileSeek(&mf.base, 4, SEEK_END+3);
  EXPECT_EQ(-1, ret_code);

  GioMemoryFileRead(&mf.base, &out_char, 1);
  EXPECT_EQ(30, out_char);

  // mf_curpos = 31
  ret_code = GioMemoryFileSeek(&mf.base, -5, SEEK_SET);
  EXPECT_EQ(-1, ret_code);

  GioMemoryFileRead(&mf.base, &out_char, 1);
  EXPECT_EQ(31, out_char);
}

}  // namespace

int main(int argc, char *argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
