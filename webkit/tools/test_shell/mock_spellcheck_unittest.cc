// Copyright (c) 2010 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "base/format_macros.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/tools/test_shell/mock_spellcheck.h"

class MockSpellCheckTest : public testing::Test {
 public:
  MockSpellCheckTest() {
  }

  ~MockSpellCheckTest() {
  }
};

TEST_F(MockSpellCheckTest, SpellCheckStrings) {
  // Test cases.
  // Even though the following test cases are copied from our unit test,
  // SpellCheckTest.SpellCheckStrings_EN_US, we omit some test cases which
  // needs to handle non-ASCII characters correctly since our MockSpellCheck
  // class cannot handle non-ASCII characters. (It just ignores non-ASCII
  // characters.)
  static const struct {
    const wchar_t* input;
    bool expected_result;
    int misspelling_start;
    int misspelling_length;
  } kTestCases[] = {
    {L"", true, 0, 0},
    {L" ", true, 0, 0},
    {L"\xA0", true, 0, 0},
    {L"\x3000", true, 0, 0},

    {L"hello", true, 0, 0},
    {L"\x4F60\x597D", true, 0, 0},
    {L"\xC548\xB155\xD558\xC138\xC694", true, 0, 0},
    {L"\x3053\x3093\x306B\x3061\x306F", true, 0, 0},
    {L"\x0930\x093E\x091C\x0927\x093E\x0928", true, 0, 0},
    {L"\xFF28\xFF45\xFF4C\xFF4C\xFF4F", true, 0, 0},
    {L"\x03B3\x03B5\x03B9\x03AC" L" " L"\x03C3\x03BF\x03C5", true, 0, 0},
    {L"\x0437\x0434\x0440\x0430\x0432\x0441"
     L"\x0442\x0432\x0443\x0439\x0442\x0435", true, 0, 0},

    {L" " L"hello", true, 0, 0},
    {L"hello" L" ", true, 0, 0},
    {L"hello" L" " L"hello", true, 0, 0},
    {L"hello:hello", true, 0, 0},

    {L"ifmmp", false, 0, 5},
    {L"_ifmmp_", false, 1, 5},

    {L" " L"ifmmp", false, 1, 5},
    {L"ifmmp" L" ", false, 0, 5},
    {L"ifmmp" L" " L"ifmmp", false, 0, 5},

    {L"qwertyuiopasd", false, 0, 13},
    {L"qwertyuiopasdf", false, 0, 14},
  };

  MockSpellCheck spellchecker;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestCases); ++i) {
    SCOPED_TRACE(StringPrintf("kTestCases[%" PRIuS "]", i));

    std::wstring input(kTestCases[i].input);
    int misspelling_start;
    int misspelling_length;
    bool result = spellchecker.SpellCheckWord(WideToUTF16(input),
                                              &misspelling_start,
                                              &misspelling_length);

    EXPECT_EQ(kTestCases[i].expected_result, result);
    EXPECT_EQ(kTestCases[i].misspelling_start, misspelling_start);
    EXPECT_EQ(kTestCases[i].misspelling_length, misspelling_length);
  }
}
