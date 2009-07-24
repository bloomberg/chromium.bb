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

#include "tests/common/win/testing_common.h"
#include "utils/cross/math_gtest.h"

namespace o3d {

class MathGTestTest : public testing::Test {
};

// Test the math gtest helper functions.
TEST_F(MathGTestTest, TestMathGTest) {
  EXPECT_EQ(Float2(1.2f, 2.3f), Float2(1.2f, 2.3f));
  EXPECT_NE(Float2(1.2f, 2.3f), Float2(1.3f, 2.3f));
  EXPECT_NE(Float2(1.2f, 2.3f), Float2(1.2f, 2.4f));
  EXPECT_EQ(Float3(1.2f, 2.3f, 4.5f), Float3(1.2f, 2.3f, 4.5f));
  EXPECT_NE(Float3(1.2f, 2.3f, 4.5f), Float3(1.3f, 2.3f, 4.5f));
  EXPECT_NE(Float3(1.2f, 2.3f, 4.5f), Float3(1.2f, 2.4f, 4.5f));
  EXPECT_NE(Float3(1.2f, 2.3f, 4.5f), Float3(1.2f, 2.3f, 4.6f));
  EXPECT_EQ(Float4(1.2f, 2.3f, 4.5f, 6.7f), Float4(1.2f, 2.3f, 4.5f, 6.7f));
  EXPECT_NE(Float4(1.2f, 2.3f, 4.5f, 6.7f), Float4(1.3f, 2.3f, 4.5f, 6.7f));
  EXPECT_NE(Float4(1.2f, 2.3f, 4.5f, 6.7f), Float4(1.2f, 2.4f, 4.5f, 6.7f));
  EXPECT_NE(Float4(1.2f, 2.3f, 4.5f, 6.7f), Float4(1.2f, 2.3f, 4.6f, 6.7f));
  EXPECT_NE(Float4(1.2f, 2.3f, 4.5f, 6.7f), Float4(1.2f, 2.3f, 4.5f, 6.8f));
  Matrix4 a(Vector4(1.1f, 2.2f, 3.3f, 4.4f),
            Vector4(1.2f, 2.3f, 3.4f, 4.5f),
            Vector4(1.3f, 2.4f, 3.5f, 4.6f),
            Vector4(1.4f, 2.5f, 3.6f, 4.7f));
  Matrix4 b(a);
  EXPECT_EQ(a, b);
  for (int ii = 0; ii < 4; ++ii) {
    for (int jj = 0; jj < 4; ++jj) {
      b = a;
      b.setElem(ii, jj, b.getElem(ii, jj) * 2);
      EXPECT_NE(a, b);
    }
  }
}

}  // namespace o3d

