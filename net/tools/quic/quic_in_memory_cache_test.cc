// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "net/spdy/spdy_framer.h"
#include "net/tools/balsa/balsa_headers.h"
#include "net/tools/quic/quic_in_memory_cache.h"
#include "net/tools/quic/test_tools/quic_in_memory_cache_peer.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::IntToString;
using base::StringPiece;
using std::string;

namespace net {
namespace tools {
namespace test {

class QuicInMemoryCacheTest : public ::testing::Test {
 protected:
  QuicInMemoryCacheTest() {
    QuicInMemoryCachePeer::ResetForTests();
  }

  ~QuicInMemoryCacheTest() override { QuicInMemoryCachePeer::ResetForTests(); }

  void CreateRequest(string host, string path, BalsaHeaders* headers) {
    headers->SetRequestFirstlineFromStringPieces("GET", path, "HTTP/1.1");
    headers->ReplaceOrAppendHeader("host", host);
  }

  string CacheDirectory() {
    base::FilePath path;
    PathService::Get(base::DIR_SOURCE_ROOT, &path);
    path = path.AppendASCII("net").AppendASCII("data")
        .AppendASCII("quic_in_memory_cache_data");
    // The file path is known to be an ascii string.
    return path.MaybeAsASCII();
  }
};

TEST_F(QuicInMemoryCacheTest, GetResponseNoMatch) {
  const QuicInMemoryCache::Response* response =
      QuicInMemoryCache::GetInstance()->GetResponse("mail.google.com",
                                                    "/index.html");
  ASSERT_FALSE(response);
}

TEST_F(QuicInMemoryCacheTest, AddSimpleResponseGetResponse) {
  string response_body("hello response");
  QuicInMemoryCache* cache = QuicInMemoryCache::GetInstance();
  cache->AddSimpleResponse("www.google.com", "/", 200, response_body);

  BalsaHeaders request_headers;
  CreateRequest("www.google.com", "/", &request_headers);
  const QuicInMemoryCache::Response* response =
      cache->GetResponse("www.google.com", "/");
  ASSERT_TRUE(response);
  ASSERT_TRUE(ContainsKey(response->headers(), ":status"));
  EXPECT_EQ("200", response->headers().find(":status")->second);
  EXPECT_EQ(response_body.size(), response->body().length());
}

TEST_F(QuicInMemoryCacheTest, ReadsCacheDir) {
  QuicInMemoryCache::GetInstance()->InitializeFromDirectory(CacheDirectory());
  const QuicInMemoryCache::Response* response =
      QuicInMemoryCache::GetInstance()->GetResponse("quic.test.url",
                                                    "/index.html");
  ASSERT_TRUE(response);
  ASSERT_TRUE(ContainsKey(response->headers(), ":status"));
  EXPECT_EQ("200 OK", response->headers().find(":status")->second);
  ASSERT_TRUE(ContainsKey(response->headers(), "connection"));
  EXPECT_EQ("close", response->headers().find("connection")->second);
  EXPECT_LT(0U, response->body().length());
}

TEST_F(QuicInMemoryCacheTest, UsesOriginalUrl) {
  QuicInMemoryCache::GetInstance()->InitializeFromDirectory(CacheDirectory());
  const QuicInMemoryCache::Response* response =
      QuicInMemoryCache::GetInstance()->GetResponse("quic.test.url",
                                                    "/index.html");
  ASSERT_TRUE(response);
  ASSERT_TRUE(ContainsKey(response->headers(), ":status"));
  EXPECT_EQ("200 OK", response->headers().find(":status")->second);
  ASSERT_TRUE(ContainsKey(response->headers(), "connection"));
  EXPECT_EQ("close", response->headers().find("connection")->second);
  EXPECT_LT(0U, response->body().length());
}

TEST_F(QuicInMemoryCacheTest, DefaultResponse) {
  // Verify GetResponse returns nullptr when no default is set.
  QuicInMemoryCache* cache = QuicInMemoryCache::GetInstance();
  const QuicInMemoryCache::Response* response =
      cache->GetResponse("www.google.com", "/");
  ASSERT_FALSE(response);

  // Add a default response.
  SpdyHeaderBlock response_headers;
  response_headers[":version"] = "HTTP/1.1";
  response_headers[":status"] = "200";
  response_headers["content-length"] = "0";
  QuicInMemoryCache::Response* default_response =
      new QuicInMemoryCache::Response;
  default_response->set_headers(response_headers);
  cache->AddDefaultResponse(default_response);

  // Now we should get the default response for the original request.
  response = cache->GetResponse("www.google.com", "/");
  ASSERT_TRUE(response);
  ASSERT_TRUE(ContainsKey(response->headers(), ":status"));
  EXPECT_EQ("200", response->headers().find(":status")->second);

  // Now add a set response for / and make sure it is returned
  cache->AddSimpleResponse("www.google.com", "/", 302, "");
  response = cache->GetResponse("www.google.com", "/");
  ASSERT_TRUE(response);
  ASSERT_TRUE(ContainsKey(response->headers(), ":status"));
  EXPECT_EQ("302", response->headers().find(":status")->second);

  // We should get the default response for other requests.
  response = cache->GetResponse("www.google.com", "/asd");
  ASSERT_TRUE(response);
  ASSERT_TRUE(ContainsKey(response->headers(), ":status"));
  EXPECT_EQ("200", response->headers().find(":status")->second);
}

}  // namespace test
}  // namespace tools
}  // namespace net
