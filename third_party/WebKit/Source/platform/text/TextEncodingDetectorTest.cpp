// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/text/TextEncodingDetector.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/TextEncoding.h"

namespace blink {

TEST(TextEncodingDetectorTest, RespectIso2022Jp) {
  // ISO-2022-JP is the only 7-bit encoding defined in WHATWG standard.
  std::string iso2022jp =
      " \x1B"
      "$BKL3$F;F|K\\%O%`%U%!%$%?!<%:$,%=%U%H%P%s%/$H$N%W%l!<%*%U$r@)$7!\"";
  WTF::TextEncoding encoding;
  bool result = detectTextEncoding(iso2022jp.c_str(), iso2022jp.length(),
                                   nullptr, nullptr, nullptr, &encoding);
  EXPECT_TRUE(result);
  EXPECT_EQ(WTF::TextEncoding("ISO-2022-JP"), encoding);
}

TEST(TextEncodingDetectorTest, Ignore7BitEncoding) {
  // 7-bit encodings except ISO-2022-JP are not supported by WHATWG.
  // They should be detected as plain text (US-ASCII).
  std::string hzGb2312 =
      " ~{\x54\x42\x31\x7D\x37\x22\x55\x39\x35\x3D\x3D\x71~} abc";
  WTF::TextEncoding encoding;
  bool result = detectTextEncoding(hzGb2312.c_str(), hzGb2312.length(), nullptr,
                                   nullptr, nullptr, &encoding);
  EXPECT_TRUE(result);
  EXPECT_EQ(WTF::TextEncoding("US-ASCII"), encoding);
}

TEST(TextEncodingDetectorTest, NonWHATWGEncodingBecomesAscii) {
  std::string pseudoJpg =
      "\xff\xd8\xff\xe0\x00\x10JFIF foo bar baz\xff\xe1\x00\xa5"
      "\x01\xd7\xff\x01\x57\x33\x44\x55\x66\x77\xed\xcb\xa9\x87"
      "\xff\xd7\xff\xe0\x00\x10JFIF foo bar baz\xff\xe1\x00\xa5"
      "\x87\x01\xd7\xff\x01\x57\x33\x44\x55\x66\x77\xed\xcb\xa9";
  WTF::TextEncoding encoding;
  bool result = detectTextEncoding(pseudoJpg.c_str(), pseudoJpg.length(),
                                   nullptr, nullptr, nullptr, &encoding);
  EXPECT_TRUE(result);
  EXPECT_EQ(WTF::TextEncoding("US-ASCII"), encoding);
}

TEST(TextEncodingDetectorTest, UrlHintHelpsEUCJP) {
  std::string eucjpBytes =
      "<TITLE>"
      "\xA5\xD1\xA5\xEF\xA1\xBC\xA5\xC1\xA5\xE3\xA1\xBC\xA5\xC8\xA1\xC3\xC5\xEA"
      "\xBB\xF1\xBE\xF0\xCA\xF3\xA4\xCE\xA5\xD5\xA5\xA3\xA5\xB9\xA5\xB3</"
      "TITLE>";
  WTF::TextEncoding encoding;
  bool result = detectTextEncoding(eucjpBytes.c_str(), eucjpBytes.length(),
                                   nullptr, nullptr, nullptr, &encoding);
  EXPECT_TRUE(result);
  EXPECT_EQ(WTF::TextEncoding("GBK"), encoding)
      << "Without language hint, it's detected as GBK";

  result = detectTextEncoding(eucjpBytes.c_str(), eucjpBytes.length(), nullptr,
                              "http://example.co.jp/", nullptr, &encoding);
  EXPECT_TRUE(result);
  EXPECT_EQ(WTF::TextEncoding("EUC-JP"), encoding)
      << "With URL hint including '.jp', it's detected as EUC-JP";
}

TEST(TextEncodingDetectorTest, LanguageHintHelpsEUCJP) {
  std::string eucjpBytes =
      "<TITLE>"
      "\xA5\xD1\xA5\xEF\xA1\xBC\xA5\xC1\xA5\xE3\xA1\xBC\xA5\xC8\xA1\xC3\xC5\xEA"
      "\xBB\xF1\xBE\xF0\xCA\xF3\xA4\xCE\xA5\xD5\xA5\xA3\xA5\xB9\xA5\xB3</"
      "TITLE>";
  WTF::TextEncoding encoding;
  bool result = detectTextEncoding(eucjpBytes.c_str(), eucjpBytes.length(),
                                   nullptr, nullptr, nullptr, &encoding);
  EXPECT_TRUE(result);
  EXPECT_EQ(WTF::TextEncoding("GBK"), encoding)
      << "Without language hint, it's detected as GBK";

  result = detectTextEncoding(eucjpBytes.c_str(), eucjpBytes.length(), nullptr,
                              nullptr, "ja", &encoding);
  EXPECT_TRUE(result);
  EXPECT_EQ(WTF::TextEncoding("EUC-JP"), encoding)
      << "With language hint 'ja', it's detected as EUC-JP";
}

TEST(TextEncodingDetectorTest, UTF8DetectionShouldFail) {
  std::string utf8Bytes =
      "tnegirjji gosa gii beare s\xC3\xA1htt\xC3\xA1 \xC4\x8D\xC3"
      "\xA1llit artihkkaliid. Maid don s\xC3\xA1ht\xC3\xA1t dievasmah";
  WTF::TextEncoding encoding;
  bool result = detectTextEncoding(utf8Bytes.c_str(), utf8Bytes.length(),
                                   nullptr, nullptr, nullptr, &encoding);
  EXPECT_FALSE(result);
}

TEST(TextEncodingDetectorTest, RespectUTF8DetectionForFileResource) {
  std::string utf8Bytes =
      "tnegirjji gosa gii beare s\xC3\xA1htt\xC3\xA1 \xC4\x8D\xC3"
      "\xA1llit artihkkaliid. Maid don s\xC3\xA1ht\xC3\xA1t dievasmah";
  WTF::TextEncoding encoding;
  bool result = detectTextEncoding(utf8Bytes.c_str(), utf8Bytes.length(),
                                   nullptr, "file:///text", nullptr, &encoding);
  EXPECT_TRUE(result);
}

}  // namespace blink
