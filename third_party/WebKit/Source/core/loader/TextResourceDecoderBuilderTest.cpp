// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/TextResourceDecoderBuilder.h"

#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

static const WTF::TextEncoding DefaultEncodingForUrlAndContentType(
    const char* url,
    const char* content_type) {
  std::unique_ptr<DummyPageHolder> page_holder =
      DummyPageHolder::Create(IntSize(0, 0));
  Document& document = page_holder->GetDocument();
  document.SetURL(KURL(NullURL(), url));
  return BuildTextResourceDecoderFor(&document, content_type, g_null_atom)
      ->Encoding();
}

static const WTF::TextEncoding DefaultEncodingForURL(const char* url) {
  return DefaultEncodingForUrlAndContentType(url, "text/html");
}

TEST(TextResourceDecoderBuilderTest, defaultEncodingForJsonIsUTF8) {
  EXPECT_EQ(WTF::TextEncoding("UTF-8"),
            DefaultEncodingForUrlAndContentType(
                "https://udarenieru.ru/1.2/dealers/", "application/json"));
}

TEST(TextResourceDecoderBuilderTest, defaultEncodingComesFromTopLevelDomain) {
  EXPECT_EQ(WTF::TextEncoding("Shift_JIS"),
            DefaultEncodingForURL("http://tsubotaa.la.coocan.jp"));
  EXPECT_EQ(WTF::TextEncoding("windows-1251"),
            DefaultEncodingForURL("http://udarenieru.ru/index.php"));
}

TEST(TextResourceDecoderBuilderTest,
     NoCountryDomainURLDefaultsToLatin1Encoding) {
  // Latin1 encoding is set in |TextResourceDecoder::defaultEncoding()|.
  EXPECT_EQ(WTF::Latin1Encoding(),
            DefaultEncodingForURL("http://arstechnica.com/about-us"));
}

}  // namespace blink
