/*
 * Copyright 2009, Google Inc.
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


// This file contains the tests of base64 functions

#include "tests/common/win/testing_common.h"
#include "utils/cross/base64.h"

namespace o3d {

class Base64Test : public testing::Test {
};

TEST_F(Base64Test, GetEncodeLength) {
  EXPECT_EQ(0u, base64::GetEncodeLength(0));
  EXPECT_EQ(4u, base64::GetEncodeLength(1));
  EXPECT_EQ(4u, base64::GetEncodeLength(2));
  EXPECT_EQ(4u, base64::GetEncodeLength(3));
  EXPECT_EQ(8u, base64::GetEncodeLength(4));
}

TEST_F(Base64Test, Encode) {
  unsigned char buffer[100];
  memset(buffer, 0xFF, sizeof(buffer));
  base64::Encode("abc", 3, buffer);
  EXPECT_EQ(0, memcmp(buffer, "YWJj", 4));
  EXPECT_EQ(0xFF, buffer[4]);
  memset(buffer, 0xFF, sizeof(buffer));
  base64::Encode("ab\0c", 4, buffer);
  EXPECT_EQ(0, memcmp(buffer, "YWIAYw==", 8));
  EXPECT_EQ(0xFF, buffer[8]);
}

TEST_F(Base64Test, GetDecodeLength) {
  size_t length = 256u;
  base64::GetDecodeLength("", 0, &length);
  EXPECT_EQ(0u, length);

  length = 256u;
  base64::GetDecodeLength("YQ==", 4, &length);
  EXPECT_EQ(1u, length);

  length = 256u;
  base64::GetDecodeLength("YWI=", 4, &length);
  EXPECT_EQ(2u, length);

  length = 256u;
  base64::GetDecodeLength("YWJj", 4, &length);
  EXPECT_EQ(3u, length);

  length = 256u;
  base64::GetDecodeLength("YWJjZA==", 8, &length);
  EXPECT_EQ(4u, length);

  length = 256u;
  base64::GetDecodeLength("YWJjZGU=", 8, &length);
  EXPECT_EQ(5u, length);
}

TEST_F(Base64Test, GetDecodeLengthInputError) {
  base64::DecodeStatus status = base64::kSuccess;
  size_t length = 256u;
  status = base64::GetDecodeLength("Y@==", 4, &length);
  EXPECT_EQ(base64::kBadCharError, status);

  status = base64::GetDecodeLength("Y Q==", 4, &length);
  EXPECT_EQ(base64::kSuccess, status);

  status = base64::GetDecodeLength("Y", 1, &length);
  EXPECT_EQ(base64::kPadError, status);
}

TEST_F(Base64Test, Decode) {
  unsigned char buffer[10];
  memset(buffer, 0xFF, sizeof(buffer));
  unsigned char result_buffer[10];
  memset(result_buffer, 0xFF, sizeof(result_buffer));
  base64::DecodeStatus status = base64::kSuccess;

  status = base64::Decode("", 0, buffer, 10);
  EXPECT_EQ(base64::kSuccess, status);
  EXPECT_EQ(0, memcmp(buffer, result_buffer, 10));

  result_buffer[0] = 'a';
  status = base64::Decode("YQ==", 4, buffer, 10);
  EXPECT_EQ(base64::kSuccess, status);
  EXPECT_EQ(0, memcmp(buffer, result_buffer, 10));

  result_buffer[1] = 'b';
  status = base64::Decode("YWI=", 4, buffer, 10);
  EXPECT_EQ(base64::kSuccess, status);
  EXPECT_EQ(0, memcmp(buffer, result_buffer, 10));

  result_buffer[2] = 'c';
  status = base64::Decode("YWJj", 4, buffer, 10);
  EXPECT_EQ(base64::kSuccess, status);
  EXPECT_EQ(0, memcmp(buffer, result_buffer, 10));

  result_buffer[3] = 'd';
  status = base64::Decode("YWJjZA==", 8, buffer, 10);
  EXPECT_EQ(base64::kSuccess, status);
  EXPECT_EQ(0, memcmp(buffer, result_buffer, 10));

  result_buffer[4] = 'e';
  status = base64::Decode("YWJjZGU=", 8, buffer, 10);
  EXPECT_EQ(base64::kSuccess, status);
  EXPECT_EQ(0, memcmp(buffer, result_buffer, 10));
}

TEST_F(Base64Test, DecodeInputError) {
  unsigned char buffer[10];
  base64::DecodeStatus status = base64::kSuccess;
  status = base64::Decode("Y@==", 4, buffer, 10);
  EXPECT_EQ(base64::kBadCharError, status);

  status = base64::Decode("Y Q==", 4, buffer, 10);
  EXPECT_EQ(base64::kSuccess, status);

  status = base64::Decode("Y", 1, buffer, 10);
  EXPECT_EQ(base64::kPadError, status);
}

TEST_F(Base64Test, DecodeOverflowError) {
  unsigned char buffer[10];
  base64::DecodeStatus status = base64::kSuccess;
  status = base64::Decode("YWJjZA==", 8, buffer, 3);
  EXPECT_EQ(base64::kOutputOverflowError, status);

  status = base64::Decode("YWJjZA==", 8, buffer, 4);
  EXPECT_EQ(base64::kSuccess, status);

  status = base64::Decode("YQ==", 8, buffer, 0);
  EXPECT_EQ(base64::kOutputOverflowError, status);

  status = base64::Decode("YQ==", 8, buffer, 1);
  EXPECT_EQ(base64::kSuccess, status);
}

}  // namespace o3d


