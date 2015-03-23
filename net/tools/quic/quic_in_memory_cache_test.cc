// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
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
    base::FilePath path;
    PathService::Get(base::DIR_SOURCE_ROOT, &path);
    path = path.AppendASCII("net").AppendASCII("data")
        .AppendASCII("quic_in_memory_cache_data");
    // The file path is known to be an ascii string.
    FLAGS_quic_in_memory_cache_dir = path.MaybeAsASCII();
  }

  ~QuicInMemoryCacheTest() override { QuicInMemoryCachePeer::ResetForTests(); }

  void CreateRequest(StringPiece host,
                     StringPiece path,
                     BalsaHeaders* headers) {
    headers->SetRequestFirstlineFromStringPieces("GET", path, "HTTP/1.1");
    headers->ReplaceOrAppendHeader("host", host);
  }

  void SetUp() override { QuicInMemoryCachePeer::ResetForTests(); }

};

TEST_F(QuicInMemoryCacheTest, AddSimpleResponseGetResponse) {
  string response_body("hello response");
  QuicInMemoryCache* cache = QuicInMemoryCache::GetInstance();
  cache->AddSimpleResponse("www.google.com", "/", 200, "OK", response_body);

  BalsaHeaders request_headers;
  CreateRequest("www.google.com", "/", &request_headers);
  const QuicInMemoryCache::Response* response =
      cache->GetResponse("www.google.com", "/");
  ASSERT_TRUE(response);
  EXPECT_EQ("200", response->headers().response_code());
  EXPECT_EQ(response_body.size(), response->body().length());
}

TEST_F(QuicInMemoryCacheTest, ReadsCacheDir) {
  const QuicInMemoryCache::Response* response =
      QuicInMemoryCache::GetInstance()->GetResponse("quic.test.url",
                                                    "/index.html");
  ASSERT_TRUE(response);
  string value;
  response->headers().GetAllOfHeaderAsString("Connection", &value);
  EXPECT_EQ("200", response->headers().response_code());
  EXPECT_EQ("Keep-Alive", value);
  EXPECT_LT(0U, response->body().length());
}

TEST_F(QuicInMemoryCacheTest, UsesOriginalUrl) {
  const QuicInMemoryCache::Response* response =
      QuicInMemoryCache::GetInstance()->GetResponse("quic.test.url",
                                                    "/index.html");
  ASSERT_TRUE(response);
  EXPECT_EQ("200", response->headers().response_code());
  string value;
  response->headers().GetAllOfHeaderAsString("Connection", &value);
  EXPECT_LT(0U, response->body().length());
}

TEST_F(QuicInMemoryCacheTest, GetResponseNoMatch) {
  const QuicInMemoryCache::Response* response =
      QuicInMemoryCache::GetInstance()->GetResponse("mail.google.com",
                                                    "/index.html");
  ASSERT_FALSE(response);
}

}  // namespace test
}  // namespace tools
}  // namespace net
