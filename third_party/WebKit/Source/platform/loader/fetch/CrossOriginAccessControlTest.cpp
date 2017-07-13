// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/loader/fetch/CrossOriginAccessControl.h"

#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/text/WTFString.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

TEST(CreateAccessControlPreflightRequestTest, LexicographicalOrder) {
  ResourceRequest request;
  request.AddHTTPHeaderField("Orange", "Orange");
  request.AddHTTPHeaderField("Apple", "Red");
  request.AddHTTPHeaderField("Kiwifruit", "Green");
  request.AddHTTPHeaderField("Content-Type", "application/octet-stream");
  request.AddHTTPHeaderField("Strawberry", "Red");

  ResourceRequest preflight =
      CrossOriginAccessControl::CreateAccessControlPreflightRequest(request);

  EXPECT_EQ("apple,content-type,kiwifruit,orange,strawberry",
            preflight.HttpHeaderField("Access-Control-Request-Headers"));
}

TEST(CreateAccessControlPreflightRequestTest, ExcludeSimpleHeaders) {
  ResourceRequest request;
  request.AddHTTPHeaderField("Accept", "everything");
  request.AddHTTPHeaderField("Accept-Language", "everything");
  request.AddHTTPHeaderField("Content-Language", "everything");
  request.AddHTTPHeaderField("Save-Data", "on");

  ResourceRequest preflight =
      CrossOriginAccessControl::CreateAccessControlPreflightRequest(request);

  // Do not emit empty-valued headers; an empty list of non-"CORS safelisted"
  // request headers should cause "Access-Control-Request-Headers:" to be
  // left out in the preflight request.
  EXPECT_EQ(g_null_atom,
            preflight.HttpHeaderField("Access-Control-Request-Headers"));
}

TEST(CreateAccessControlPreflightRequestTest, ExcludeSimpleContentTypeHeader) {
  ResourceRequest request;
  request.AddHTTPHeaderField("Content-Type", "text/plain");

  ResourceRequest preflight =
      CrossOriginAccessControl::CreateAccessControlPreflightRequest(request);

  // Empty list also; see comment in test above.
  EXPECT_EQ(g_null_atom,
            preflight.HttpHeaderField("Access-Control-Request-Headers"));
}

TEST(CreateAccessControlPreflightRequestTest, IncludeNonSimpleHeader) {
  ResourceRequest request;
  request.AddHTTPHeaderField("X-Custom-Header", "foobar");

  ResourceRequest preflight =
      CrossOriginAccessControl::CreateAccessControlPreflightRequest(request);

  EXPECT_EQ("x-custom-header",
            preflight.HttpHeaderField("Access-Control-Request-Headers"));
}

TEST(CreateAccessControlPreflightRequestTest,
     IncludeNonSimpleContentTypeHeader) {
  ResourceRequest request;
  request.AddHTTPHeaderField("Content-Type", "application/octet-stream");

  ResourceRequest preflight =
      CrossOriginAccessControl::CreateAccessControlPreflightRequest(request);

  EXPECT_EQ("content-type",
            preflight.HttpHeaderField("Access-Control-Request-Headers"));
}

TEST(ParseAccessControlExposeHeadersAllowListTest, ValidInput) {
  HTTPHeaderSet set;
  CrossOriginAccessControl::ParseAccessControlExposeHeadersAllowList("valid",
                                                                     set);
  EXPECT_EQ(1U, set.size());
  EXPECT_TRUE(set.Contains("valid"));

  set.clear();
  CrossOriginAccessControl::ParseAccessControlExposeHeadersAllowList("a,b",
                                                                     set);
  EXPECT_EQ(2U, set.size());
  EXPECT_TRUE(set.Contains("a"));
  EXPECT_TRUE(set.Contains("b"));

  set.clear();
  CrossOriginAccessControl::ParseAccessControlExposeHeadersAllowList(
      "   a ,  b ", set);
  EXPECT_EQ(2U, set.size());
  EXPECT_TRUE(set.Contains("a"));
  EXPECT_TRUE(set.Contains("b"));

  set.clear();
  CrossOriginAccessControl::ParseAccessControlExposeHeadersAllowList(
      " \t   \t\t a", set);
  EXPECT_EQ(1U, set.size());
  EXPECT_TRUE(set.Contains("a"));
}

TEST(ParseAccessControlExposeHeadersAllowListTest, DuplicatedEntries) {
  HTTPHeaderSet set;
  CrossOriginAccessControl::ParseAccessControlExposeHeadersAllowList("a, a",
                                                                     set);
  EXPECT_EQ(1U, set.size());
  EXPECT_TRUE(set.Contains("a"));

  set.clear();
  CrossOriginAccessControl::ParseAccessControlExposeHeadersAllowList("a, a, b",
                                                                     set);
  EXPECT_EQ(2U, set.size());
  EXPECT_TRUE(set.Contains("a"));
  EXPECT_TRUE(set.Contains("b"));
}

TEST(ParseAccessControlExposeHeadersAllowListTest, InvalidInput) {
  HTTPHeaderSet set;
  CrossOriginAccessControl::ParseAccessControlExposeHeadersAllowList(
      "not valid", set);
  EXPECT_TRUE(set.IsEmpty());

  set.clear();
  CrossOriginAccessControl::ParseAccessControlExposeHeadersAllowList("///",
                                                                     set);
  EXPECT_TRUE(set.IsEmpty());

  set.clear();
  CrossOriginAccessControl::ParseAccessControlExposeHeadersAllowList("/a/",
                                                                     set);
  EXPECT_TRUE(set.IsEmpty());

  set.clear();
  CrossOriginAccessControl::ParseAccessControlExposeHeadersAllowList(",", set);
  EXPECT_TRUE(set.IsEmpty());

  set.clear();
  CrossOriginAccessControl::ParseAccessControlExposeHeadersAllowList(" , ",
                                                                     set);
  EXPECT_TRUE(set.IsEmpty());

  set.clear();
  CrossOriginAccessControl::ParseAccessControlExposeHeadersAllowList(" , a",
                                                                     set);
  EXPECT_TRUE(set.IsEmpty());

  set.clear();
  CrossOriginAccessControl::ParseAccessControlExposeHeadersAllowList("a , ",
                                                                     set);
  EXPECT_TRUE(set.IsEmpty());

  set.clear();
  CrossOriginAccessControl::ParseAccessControlExposeHeadersAllowList("", set);
  EXPECT_TRUE(set.IsEmpty());

  set.clear();
  CrossOriginAccessControl::ParseAccessControlExposeHeadersAllowList(" ", set);
  EXPECT_TRUE(set.IsEmpty());

  set.clear();
  // U+0141 which is 'A' (0x41) + 0x100.
  CrossOriginAccessControl::ParseAccessControlExposeHeadersAllowList(
      String::FromUTF8("\xC5\x81"), set);
  EXPECT_TRUE(set.IsEmpty());
}

}  // namespace

}  // namespace blink
