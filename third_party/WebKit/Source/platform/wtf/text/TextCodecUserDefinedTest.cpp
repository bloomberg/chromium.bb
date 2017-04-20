// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/wtf/text/TextCodecUserDefined.h"

#include "platform/wtf/text/CString.h"
#include "platform/wtf/text/TextCodec.h"
#include "platform/wtf/text/TextEncoding.h"
#include "platform/wtf/text/TextEncodingRegistry.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

TEST(TextCodecUserDefinedTest, QuestionMarksAndSurrogates) {
  WTF::TextEncoding encoding("x-user-defined");
  std::unique_ptr<WTF::TextCodec> codec(NewTextCodec(encoding));

  {
    const LChar testCase[] = {0xd1, 0x16, 0x86};
    size_t testCaseSize = WTF_ARRAY_LENGTH(testCase);
    CString result = codec->Encode(testCase, testCaseSize,
                                   WTF::kQuestionMarksForUnencodables);
    EXPECT_STREQ("?\x16?", result.data());
  }
  {
    const UChar testCase[] = {0xd9f0, 0xdcd9};
    size_t testCaseSize = WTF_ARRAY_LENGTH(testCase);
    CString result = codec->Encode(testCase, testCaseSize,
                                   WTF::kQuestionMarksForUnencodables);
    EXPECT_STREQ("?", result.data());
  }
  {
    const UChar testCase[] = {0xd9f0, 0xdcd9, 0xd9f0, 0xdcd9};
    size_t testCaseSize = WTF_ARRAY_LENGTH(testCase);
    CString result = codec->Encode(testCase, testCaseSize,
                                   WTF::kQuestionMarksForUnencodables);
    EXPECT_STREQ("??", result.data());
  }
  {
    const UChar testCase[] = {0x0041, 0xd9f0, 0xdcd9, 0xf7c1, 0xd9f0, 0xdcd9};
    size_t testCaseSize = WTF_ARRAY_LENGTH(testCase);
    CString result = codec->Encode(testCase, testCaseSize,
                                   WTF::kQuestionMarksForUnencodables);
    EXPECT_STREQ("A?\xC1?", result.data());
  }
}
