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


// This file contains the tests of dataurl functions

#include "tests/common/win/testing_common.h"
#include "utils/cross/dataurl.h"

namespace o3d {

class DataURLTest : public testing::Test {
};

TEST_F(DataURLTest, kEmptyDataURL) {
  EXPECT_STREQ("data:,", dataurl::kEmptyDataURL);
}

TEST_F(DataURLTest, ToDataURL) {
  EXPECT_STREQ("data:a/b;base64,YWJj",
               dataurl::ToDataURL("a/b", "abc", 3).c_str());
  EXPECT_STREQ("data:de/ej;base64,YWIAYw==",
               dataurl::ToDataURL("de/ej", "ab\0c", 4).c_str());
}

TEST_F(DataURLTest, FromDataURL) {
  String data_url("data:a/b;base64,YWJj");
  scoped_array<uint8> output;
  size_t output_length;
  String error_string;

  EXPECT_TRUE(dataurl::FromDataURL(data_url,
                                    &output,
                                    &output_length,
                                    &error_string));
  EXPECT_EQ(3, output_length);
  EXPECT_EQ(0, memcmp("abc", output.get(), 3));
}

TEST_F(DataURLTest, FromDataURLFormatErrors) {
  scoped_array<uint8> output;
  size_t output_length;
  String error_string("");
  // Not long enough
  EXPECT_FALSE(dataurl::FromDataURL("",
                                    &output,
                                    &output_length,
                                    &error_string));
  EXPECT_LT(0u, error_string.size());
  // Does not start with "data:"
  error_string = "";
  EXPECT_FALSE(dataurl::FromDataURL("aaaaaaaaaaaaaaaa",
                                    &output,
                                    &output_length,
                                    &error_string));
  EXPECT_LT(0u, error_string.size());
  // Must contain base64
  error_string = "";
  EXPECT_FALSE(dataurl::FromDataURL("data:aaaaaaaaaaa",
                                    &output,
                                    &output_length,
                                    &error_string));
  EXPECT_LT(0u, error_string.size());
  // Must contain data.
  error_string = "";
  EXPECT_FALSE(dataurl::FromDataURL("data:aa;base64,",
                                    &output,
                                    &output_length,
                                    &error_string));
  EXPECT_LT(0u, error_string.size());

  // Bad character in data.
  error_string = "";
  EXPECT_FALSE(dataurl::FromDataURL("data:;base64,@",
                                    &output,
                                    &output_length,
                                    &error_string));
  EXPECT_LT(0u, error_string.size());
  // Padding error in data.
  error_string = "";
  EXPECT_FALSE(dataurl::FromDataURL("data:;base64,Y",
                                    &output,
                                    &output_length,
                                    &error_string));
  EXPECT_LT(0u, error_string.size());
  // Correct.
  error_string = "";
  EXPECT_TRUE(dataurl::FromDataURL("data:;base64,YWJj",
                                    &output,
                                    &output_length,
                                    &error_string));
  EXPECT_EQ(0u, error_string.size());
}

}  // namespace o3d


