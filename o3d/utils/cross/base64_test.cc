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
  EXPECT_EQ(0, base64::GetEncodeLength(0));
  EXPECT_EQ(4, base64::GetEncodeLength(1));
  EXPECT_EQ(4, base64::GetEncodeLength(2));
  EXPECT_EQ(4, base64::GetEncodeLength(3));
  EXPECT_EQ(8, base64::GetEncodeLength(4));
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

}  // namespace o3d


