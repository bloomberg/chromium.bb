// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebCORS.h"

#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

TEST(CreateAccessControlPreflightRequestTest, LexicographicalOrder) {
  WebURLRequest request;
  request.AddHTTPHeaderField("Orange", "Orange");
  request.AddHTTPHeaderField("Apple", "Red");
  request.AddHTTPHeaderField("Kiwifruit", "Green");
  request.AddHTTPHeaderField("Content-Type", "application/octet-stream");
  request.AddHTTPHeaderField("Strawberry", "Red");

  WebURLRequest preflight =
      WebCORS::CreateAccessControlPreflightRequest(request);

  EXPECT_EQ("apple,content-type,kiwifruit,orange,strawberry",
            preflight.HttpHeaderField("Access-Control-Request-Headers"));
}

TEST(CreateAccessControlPreflightRequestTest, ExcludeSimpleHeaders) {
  WebURLRequest request;
  request.AddHTTPHeaderField("Accept", "everything");
  request.AddHTTPHeaderField("Accept-Language", "everything");
  request.AddHTTPHeaderField("Content-Language", "everything");
  request.AddHTTPHeaderField("Save-Data", "on");

  WebURLRequest preflight =
      WebCORS::CreateAccessControlPreflightRequest(request);

  // Do not emit empty-valued headers; an empty list of non-"CORS safelisted"
  // request headers should cause "Access-Control-Request-Headers:" to be
  // left out in the preflight request.
  EXPECT_EQ(WebString(g_null_atom),
            preflight.HttpHeaderField("Access-Control-Request-Headers"));
}

TEST(CreateAccessControlPreflightRequestTest, ExcludeSimpleContentTypeHeader) {
  WebURLRequest request;
  request.AddHTTPHeaderField("Content-Type", "text/plain");

  WebURLRequest preflight =
      WebCORS::CreateAccessControlPreflightRequest(request);

  // Empty list also; see comment in test above.
  EXPECT_EQ(WebString(g_null_atom),
            preflight.HttpHeaderField("Access-Control-Request-Headers"));
}

TEST(CreateAccessControlPreflightRequestTest, IncludeNonSimpleHeader) {
  WebURLRequest request;
  request.AddHTTPHeaderField("X-Custom-Header", "foobar");

  WebURLRequest preflight =
      WebCORS::CreateAccessControlPreflightRequest(request);

  EXPECT_EQ("x-custom-header",
            preflight.HttpHeaderField("Access-Control-Request-Headers"));
}

TEST(CreateAccessControlPreflightRequestTest,
     IncludeNonSimpleContentTypeHeader) {
  WebURLRequest request;
  request.AddHTTPHeaderField("Content-Type", "application/octet-stream");

  WebURLRequest preflight =
      WebCORS::CreateAccessControlPreflightRequest(request);

  EXPECT_EQ("content-type",
            preflight.HttpHeaderField("Access-Control-Request-Headers"));
}

TEST(ParseAccessControlExposeHeadersAllowListTest, ValidInput) {
  WebHTTPHeaderSet set;
  WebCORS::ParseAccessControlExposeHeadersAllowList("valid", set);
  EXPECT_EQ(1U, set.size());
  EXPECT_TRUE(set.find("valid") != set.end());

  set.clear();
  WebCORS::ParseAccessControlExposeHeadersAllowList("a,b", set);
  EXPECT_EQ(2U, set.size());
  EXPECT_TRUE(set.find("a") != set.end());
  EXPECT_TRUE(set.find("b") != set.end());

  set.clear();
  WebCORS::ParseAccessControlExposeHeadersAllowList("   a ,  b ", set);
  EXPECT_EQ(2U, set.size());
  EXPECT_TRUE(set.find("a") != set.end());
  EXPECT_TRUE(set.find("b") != set.end());

  set.clear();
  WebCORS::ParseAccessControlExposeHeadersAllowList(" \t   \t\t a", set);
  EXPECT_EQ(1U, set.size());
  EXPECT_TRUE(set.find("a") != set.end());
}

TEST(ParseAccessControlExposeHeadersAllowListTest, DuplicatedEntries) {
  WebHTTPHeaderSet set;
  WebCORS::ParseAccessControlExposeHeadersAllowList("a, a", set);
  EXPECT_EQ(1U, set.size());
  EXPECT_TRUE(set.find("a") != set.end());

  set.clear();
  WebCORS::ParseAccessControlExposeHeadersAllowList("a, a, b", set);
  EXPECT_EQ(2U, set.size());
  EXPECT_TRUE(set.find("a") != set.end());
  EXPECT_TRUE(set.find("b") != set.end());
}

TEST(ParseAccessControlExposeHeadersAllowListTest, InvalidInput) {
  WebHTTPHeaderSet set;
  WebCORS::ParseAccessControlExposeHeadersAllowList("not valid", set);
  EXPECT_TRUE(set.empty());

  set.clear();
  WebCORS::ParseAccessControlExposeHeadersAllowList("///", set);
  EXPECT_TRUE(set.empty());

  set.clear();
  WebCORS::ParseAccessControlExposeHeadersAllowList("/a/", set);
  EXPECT_TRUE(set.empty());

  set.clear();
  WebCORS::ParseAccessControlExposeHeadersAllowList(",", set);
  EXPECT_TRUE(set.empty());

  set.clear();
  WebCORS::ParseAccessControlExposeHeadersAllowList(" , ", set);
  EXPECT_TRUE(set.empty());

  set.clear();
  WebCORS::ParseAccessControlExposeHeadersAllowList(" , a", set);
  EXPECT_TRUE(set.empty());

  set.clear();
  WebCORS::ParseAccessControlExposeHeadersAllowList("a , ", set);
  EXPECT_TRUE(set.empty());

  set.clear();
  WebCORS::ParseAccessControlExposeHeadersAllowList("", set);
  EXPECT_TRUE(set.empty());

  set.clear();
  WebCORS::ParseAccessControlExposeHeadersAllowList(" ", set);
  EXPECT_TRUE(set.empty());

  set.clear();
  // U+0141 which is 'A' (0x41) + 0x100.
  WebCORS::ParseAccessControlExposeHeadersAllowList(
      String::FromUTF8("\xC5\x81"), set);
  EXPECT_TRUE(set.empty());
}

}  // namespace

}  // namespace blink
