// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebCORS.h"

#include "platform/exported/WrappedResourceResponse.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/loader/fetch/ResourceResponse.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class CORSExposedHeadersTest : public ::testing::Test {
 public:
  using CredentialsMode = network::mojom::FetchCredentialsMode;

  WebHTTPHeaderSet Parse(CredentialsMode credentials_mode,
                         const AtomicString& header) const {
    ResourceResponse response;
    response.AddHTTPHeaderField("access-control-expose-headers", header);

    return WebCORS::ExtractCorsExposedHeaderNamesList(
        credentials_mode, WrappedResourceResponse(response));
  }
};

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

TEST_F(CORSExposedHeadersTest, ValidInput) {
  EXPECT_EQ(Parse(CredentialsMode::kOmit, "valid"),
            WebHTTPHeaderSet({"valid"}));

  EXPECT_EQ(Parse(CredentialsMode::kOmit, "a,b"), WebHTTPHeaderSet({"a", "b"}));

  EXPECT_EQ(Parse(CredentialsMode::kOmit, "   a ,  b "),
            WebHTTPHeaderSet({"a", "b"}));

  EXPECT_EQ(Parse(CredentialsMode::kOmit, " \t   \t\t a"),
            WebHTTPHeaderSet({"a"}));
}

TEST_F(CORSExposedHeadersTest, DuplicatedEntries) {
  EXPECT_EQ(Parse(CredentialsMode::kOmit, "a, a"), WebHTTPHeaderSet{"a"});

  EXPECT_EQ(Parse(CredentialsMode::kOmit, "a, a, b"),
            WebHTTPHeaderSet({"a", "b"}));
}

TEST_F(CORSExposedHeadersTest, InvalidInput) {
  EXPECT_TRUE(Parse(CredentialsMode::kOmit, "not valid").empty());

  EXPECT_TRUE(Parse(CredentialsMode::kOmit, "///").empty());

  EXPECT_TRUE(Parse(CredentialsMode::kOmit, "/a/").empty());

  EXPECT_TRUE(Parse(CredentialsMode::kOmit, ",").empty());

  EXPECT_TRUE(Parse(CredentialsMode::kOmit, " , ").empty());

  EXPECT_TRUE(Parse(CredentialsMode::kOmit, " , a").empty());

  EXPECT_TRUE(Parse(CredentialsMode::kOmit, "a , ").empty());

  EXPECT_TRUE(Parse(CredentialsMode::kOmit, "").empty());

  EXPECT_TRUE(Parse(CredentialsMode::kOmit, " ").empty());

  // U+0141 which is 'A' (0x41) + 0x100.
  EXPECT_TRUE(
      Parse(CredentialsMode::kOmit, AtomicString(String::FromUTF8("\xC5\x81")))
          .empty());
}

TEST_F(CORSExposedHeadersTest, Wildcard) {
  ResourceResponse response;
  response.AddHTTPHeaderField("access-control-expose-headers", "a, b, *");
  response.AddHTTPHeaderField("b", "-");
  response.AddHTTPHeaderField("c", "-");
  response.AddHTTPHeaderField("d", "-");
  response.AddHTTPHeaderField("*", "-");

  EXPECT_EQ(
      WebCORS::ExtractCorsExposedHeaderNamesList(
          CredentialsMode::kOmit, WrappedResourceResponse(response)),
      WebHTTPHeaderSet({"access-control-expose-headers", "b", "c", "d", "*"}));

  EXPECT_EQ(
      WebCORS::ExtractCorsExposedHeaderNamesList(
          CredentialsMode::kSameOrigin, WrappedResourceResponse(response)),
      WebHTTPHeaderSet({"access-control-expose-headers", "b", "c", "d", "*"}));
}

TEST_F(CORSExposedHeadersTest, Asterisk) {
  ResourceResponse response;
  response.AddHTTPHeaderField("access-control-expose-headers", "a, b, *");
  response.AddHTTPHeaderField("b", "-");
  response.AddHTTPHeaderField("c", "-");
  response.AddHTTPHeaderField("d", "-");
  response.AddHTTPHeaderField("*", "-");

  EXPECT_EQ(WebCORS::ExtractCorsExposedHeaderNamesList(
                CredentialsMode::kInclude, WrappedResourceResponse(response)),
            WebHTTPHeaderSet({"a", "b", "*"}));
}

}  // namespace

}  // namespace blink
