// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRegularExpression.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextCaseSensitivity.h"

using WebKit::WebRegularExpression;
using WebKit::WebString;
using WebKit::WebTextCaseInsensitive;
using WebKit::WebTextCaseSensitive;
using WebKit::WebUChar;

namespace {

class RegexTest : public testing::Test {
};

struct Match {
  const WebUChar* text;
  const int textLength;
  const int matchPosition;
  const int matchLength;
};

void testMatches(const WebRegularExpression& regex,
                 const Match* matches,
                 const size_t nMatches) {

  for (size_t i = 0; i < nMatches; ++i) {
    int matchedLength = matches[i].textLength;
    EXPECT_EQ(matches[i].matchPosition, regex.match(
        WebString(matches[i].text, matches[i].textLength), 0, &matchedLength));
    if (matches[i].matchPosition != -1)
      EXPECT_EQ(matches[i].matchLength, matchedLength);
  }

}

}  // namespace

#define MATCH_DESC(webuchar, matchPosition, matchLength) \
    { webuchar, arraysize(webuchar), matchPosition, matchLength }


TEST(RegexTest, Basic) {
  // Just make sure we're not completely broken.
  WebRegularExpression regex("the quick brown fox", WebTextCaseSensitive);
  EXPECT_EQ(0, regex.match("the quick brown fox"));
  EXPECT_EQ(1, regex.match(" the quick brown fox"));
  EXPECT_EQ(3, regex.match("foothe quick brown foxbar"));

  EXPECT_EQ(-1, regex.match("The quick brown FOX"));
  EXPECT_EQ(-1, regex.match("the quick brown fo"));
}

TEST(RegexTest, Unicode) {
  // Make sure we get the right offsets for unicode strings.
  WebUChar pattern[] = {L'\x6240', L'\x6709', L'\x7f51', L'\x9875'};
  WebRegularExpression regex(WebString(pattern, arraysize(pattern)),
                             WebTextCaseInsensitive);

  WebUChar text1[] = {L'\x6240', L'\x6709', L'\x7f51', L'\x9875'};
  WebUChar text2[] = {L' ', L'\x6240', L'\x6709', L'\x7f51', L'\x9875'};
  WebUChar text3[] = {L'f', L'o', L'o', L'\x6240', L'\x6709', L'\x7f51', L'\x9875', L'b', L'a', L'r'};
  WebUChar text4[] = {L'\x4e2d', L'\x6587', L'\x7f51',  L'\x9875', L'\x6240', L'\x6709', L'\x7f51', L'\x9875'};

  const Match matches[] = {
    MATCH_DESC(text1, 0, 4),
    MATCH_DESC(text2, 1, 4),
    MATCH_DESC(text3, 3, 4),
    MATCH_DESC(text4, 4, 4),
  };

  testMatches(regex, matches, arraysize(matches));
}

TEST(RegexTest, UnicodeMixedLength) {
  WebUChar pattern[] = {L':', L'[', L' ', L'\x2000', L']', L'+', L':'};
  WebRegularExpression regex(WebString(pattern, arraysize(pattern)),
                             WebTextCaseInsensitive);

  WebUChar text1[] = {L':', L' ', L' ', L':'};
  WebUChar text2[] = {L' ', L' ', L':', L' ', L' ', L' ', L' ', L':', L' ', L' '};
  WebUChar text3[] = {L' ', L':', L' ', L'\x2000', L' ', L':', L' '};
  WebUChar text4[] = {L'\x6240', L'\x6709', L'\x7f51', L'\x9875', L' ', L':', L' ', L'\x2000', L' ', L'\x2000', L' ', L':', L' '};
  WebUChar text5[] = {L' '};
  WebUChar text6[] = {L':', L':'};

  const Match matches[] = {
    MATCH_DESC(text1, 0, 4),
    MATCH_DESC(text2, 2, 6),
    MATCH_DESC(text3, 1, 5),
    MATCH_DESC(text4, 5, 7),
    MATCH_DESC(text5, -1, -1),
    MATCH_DESC(text6, -1, -1),
  };

  testMatches(regex, matches, arraysize(matches));
}

TEST(RegexTest, EmptyMatch) {
  WebRegularExpression regex("|x", WebTextCaseInsensitive);
  int matchedLength = 0;
  EXPECT_EQ(0, regex.match("", 0, &matchedLength));
  EXPECT_EQ(0, matchedLength);
}
