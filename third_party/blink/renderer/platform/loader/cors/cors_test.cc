// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/cors/cors.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"

namespace blink {

namespace {

class CorsExposedHeadersTest : public testing::Test {
 public:
  using CredentialsMode = network::mojom::FetchCredentialsMode;

  WebHTTPHeaderSet Parse(CredentialsMode credentials_mode,
                         const AtomicString& header) const {
    ResourceResponse response;
    response.AddHTTPHeaderField("access-control-expose-headers", header);

    return cors::ExtractCorsExposedHeaderNamesList(credentials_mode, response);
  }
};

TEST_F(CorsExposedHeadersTest, ValidInput) {
  EXPECT_EQ(Parse(CredentialsMode::kOmit, "valid"),
            WebHTTPHeaderSet({"valid"}));

  EXPECT_EQ(Parse(CredentialsMode::kOmit, "a,b"), WebHTTPHeaderSet({"a", "b"}));

  EXPECT_EQ(Parse(CredentialsMode::kOmit, "   a ,  b "),
            WebHTTPHeaderSet({"a", "b"}));

  EXPECT_EQ(Parse(CredentialsMode::kOmit, " \t   \t\t a"),
            WebHTTPHeaderSet({"a"}));
}

TEST_F(CorsExposedHeadersTest, DuplicatedEntries) {
  EXPECT_EQ(Parse(CredentialsMode::kOmit, "a, a"), WebHTTPHeaderSet{"a"});

  EXPECT_EQ(Parse(CredentialsMode::kOmit, "a, a, b"),
            WebHTTPHeaderSet({"a", "b"}));
}

TEST_F(CorsExposedHeadersTest, InvalidInput) {
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

TEST_F(CorsExposedHeadersTest, Wildcard) {
  ResourceResponse response;
  response.AddHTTPHeaderField("access-control-expose-headers", "a, b, *");
  response.AddHTTPHeaderField("b", "-");
  response.AddHTTPHeaderField("c", "-");
  response.AddHTTPHeaderField("d", "-");
  response.AddHTTPHeaderField("*", "-");

  EXPECT_EQ(
      cors::ExtractCorsExposedHeaderNamesList(CredentialsMode::kOmit, response),
      WebHTTPHeaderSet({"access-control-expose-headers", "b", "c", "d", "*"}));

  EXPECT_EQ(
      cors::ExtractCorsExposedHeaderNamesList(CredentialsMode::kSameOrigin,
                                              response),
      WebHTTPHeaderSet({"access-control-expose-headers", "b", "c", "d", "*"}));
}

TEST_F(CorsExposedHeadersTest, Asterisk) {
  ResourceResponse response;
  response.AddHTTPHeaderField("access-control-expose-headers", "a, b, *");
  response.AddHTTPHeaderField("b", "-");
  response.AddHTTPHeaderField("c", "-");
  response.AddHTTPHeaderField("d", "-");
  response.AddHTTPHeaderField("*", "-");

  EXPECT_EQ(cors::ExtractCorsExposedHeaderNamesList(CredentialsMode::kInclude,
                                                    response),
            WebHTTPHeaderSet({"a", "b", "*"}));
}

}  // namespace

}  // namespace blink
