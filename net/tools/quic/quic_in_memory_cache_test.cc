// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_in_memory_cache.h"

#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "base/path_service.h"
#include "net/tools/flip_server/balsa_headers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace tools {
namespace test {
namespace {

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

  void CreateRequest(std::string host,
                     std::string path,
                     net::BalsaHeaders* headers) {
    headers->SetRequestFirstlineFromStringPieces("GET", path, "HTTP/1.1");
    headers->ReplaceOrAppendHeader("host", host);
  }
};

TEST_F(QuicInMemoryCacheTest, ReadsCacheDir) {
  net::BalsaHeaders request_headers;
  CreateRequest("quic.test.url", "/index.html", &request_headers);

  const QuicInMemoryCache::Response* response =
      QuicInMemoryCache::GetInstance()->GetResponse(request_headers);
  ASSERT_TRUE(response);
  std::string value;
  response->headers().GetAllOfHeaderAsString("Connection", &value);
  EXPECT_EQ("200", response->headers().response_code());
  EXPECT_EQ("Keep-Alive", value);
  EXPECT_LT(0U, response->body().length());
}

TEST_F(QuicInMemoryCacheTest, ReadsCacheDirHttp) {
  net::BalsaHeaders request_headers;
  CreateRequest("http://quic.test.url", "/index.html", &request_headers);

  const QuicInMemoryCache::Response* response =
      QuicInMemoryCache::GetInstance()->GetResponse(request_headers);
  ASSERT_TRUE(response);
  std::string value;
  response->headers().GetAllOfHeaderAsString("Connection", &value);
  EXPECT_EQ("200", response->headers().response_code());
  EXPECT_EQ("Keep-Alive", value);
  EXPECT_LT(0U, response->body().length());
}

TEST_F(QuicInMemoryCacheTest, GetResponseNoMatch) {
  net::BalsaHeaders request_headers;
  CreateRequest("www.google.com", "/index.html", &request_headers);

  const QuicInMemoryCache::Response* response =
      QuicInMemoryCache::GetInstance()->GetResponse(request_headers);
  ASSERT_FALSE(response);
}

}  // namespace
}  // namespace test
}  // namespace tools
}  // namespace net
