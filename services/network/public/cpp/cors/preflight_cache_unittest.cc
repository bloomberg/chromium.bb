// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cors/preflight_cache.h"

#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "net/http/http_request_headers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network {

namespace cors {

namespace {

class PreflightCacheTest : public testing::Test {
 public:
  PreflightCacheTest() = default;

 protected:
  size_t cache_size() const { return cache_.size_for_testing(); }
  PreflightCache* cache() { return &cache_; }

  void AppendEntry(const std::string& origin, const GURL& url) {
    std::unique_ptr<PreflightResult> result = PreflightResult::Create(
        mojom::FetchCredentialsMode::kInclude, std::string("POST"),
        base::nullopt, std::string("5"), nullptr);
    cache_.AppendEntry(origin, url, std::move(result));
  }

  bool CheckEntryAndRefreshCache(const std::string& origin, const GURL& url) {
    return cache_.CheckIfRequestCanSkipPreflight(
        origin, url, network::mojom::FetchCredentialsMode::kInclude, "POST",
        net::HttpRequestHeaders());
  }

  void Advance(int seconds) {
    clock_.Advance(base::TimeDelta::FromSeconds(seconds));
  }

 private:
  // testing::Test implementation.
  void SetUp() override { PreflightResult::SetTickClockForTesting(&clock_); }
  void TearDown() override { PreflightResult::SetTickClockForTesting(nullptr); }

  PreflightCache cache_;
  base::SimpleTestTickClock clock_;
};

TEST_F(PreflightCacheTest, CacheSize) {
  const std::string origin("null");
  const std::string other_origin("http://www.other.com:80");
  const GURL url("http://www.test.com/A");
  const GURL other_url("http://www.test.com/B");

  // Cache should be empty.
  EXPECT_EQ(0u, cache_size());

  AppendEntry(origin, url);

  // Cache size should be 2 (counting origins and urls separately).
  EXPECT_EQ(2u, cache_size());

  AppendEntry(origin, other_url);

  // Cache size should now be 3 (1 origin, 2 urls).
  EXPECT_EQ(3u, cache_size());

  AppendEntry(other_origin, url);

  // Cache size should now be 4 (4 origin, 3 urls).
  EXPECT_EQ(5u, cache_size());
}

TEST_F(PreflightCacheTest, CacheTimeout) {
  const std::string origin("null");
  const GURL url("http://www.test.com/A");
  const GURL other_url("http://www.test.com/B");

  // Cache should be empty.
  EXPECT_EQ(0u, cache_size());

  AppendEntry(origin, url);
  AppendEntry(origin, other_url);

  // Cache size should be 3 (counting origins and urls separately).
  EXPECT_EQ(3u, cache_size());

  // Cache entry should still be valid.
  EXPECT_TRUE(CheckEntryAndRefreshCache(origin, url));

  // Advance time by ten seconds.
  Advance(10);

  // Cache entry should now be expired.
  EXPECT_FALSE(CheckEntryAndRefreshCache(origin, url));

  // Cache size should be 2, with the expired entry removed, but one origin and
  // one url still in the cache.
  EXPECT_EQ(2u, cache_size());

  // Cache entry should be expired.
  EXPECT_FALSE(CheckEntryAndRefreshCache(origin, other_url));

  // Cache size should be 0, with the expired entry removed.
  EXPECT_EQ(0u, cache_size());
}

}  // namespace

}  // namespace cors

}  // namespace network
