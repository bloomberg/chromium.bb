// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cors/preflight_cache.h"

#include "base/test/scoped_task_environment.h"
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
  size_t CountEntries() const { return cache_.CountEntriesForTesting(); }
  void MayPurge(size_t max_entries, size_t purge_unit) {
    cache_.MayPurgeForTesting(max_entries, purge_unit);
  }
  PreflightCache* cache() { return &cache_; }

  std::unique_ptr<PreflightResult> CreateEntry() {
    return PreflightResult::Create(mojom::FetchCredentialsMode::kInclude,
                                   std::string("POST"), base::nullopt,
                                   std::string("5"), nullptr);
  }

  void AppendEntry(const std::string& origin, const GURL& url) {
    cache_.AppendEntry(origin, url, CreateEntry());
  }

  bool CheckEntryAndRefreshCache(const std::string& origin, const GURL& url) {
    return cache_.CheckIfRequestCanSkipPreflight(
        origin, url, network::mojom::FetchCredentialsMode::kInclude, "POST",
        net::HttpRequestHeaders(), false);
  }

  void Advance(int seconds) {
    clock_.Advance(base::TimeDelta::FromSeconds(seconds));
  }

  size_t EstimateMemoryPressure() {
    PreflightCache::Metrics metrics = cache_.ReportAndGatherSizeMetric();
    return metrics.memory_pressure_in_bytes;
  }

 private:
  // testing::Test implementation.
  void SetUp() override { PreflightResult::SetTickClockForTesting(&clock_); }
  void TearDown() override { PreflightResult::SetTickClockForTesting(nullptr); }

  base::test::ScopedTaskEnvironment env_;
  PreflightCache cache_;
  base::SimpleTestTickClock clock_;
};

TEST_F(PreflightCacheTest, CacheSize) {
  const std::string origin("null");
  const std::string other_origin("http://www.other.com:80");
  const GURL url("http://www.test.com/A");
  const GURL other_url("http://www.test.com/B");

  EXPECT_EQ(0u, CountEntries());

  AppendEntry(origin, url);

  EXPECT_EQ(1u, CountEntries());

  AppendEntry(origin, other_url);

  EXPECT_EQ(2u, CountEntries());

  AppendEntry(other_origin, url);

  EXPECT_EQ(3u, CountEntries());

  // Num of entries is 3, that is not greater than the limit 3u.
  // It results in doing nothing.
  MayPurge(3u, 2u);
  EXPECT_EQ(3u, CountEntries());

  // Num of entries is 3, that is greater than the limit 2u.
  // It results in purging entries by the specified unit 2u, thus only one entry
  // remains.
  MayPurge(2u, 2u);
  EXPECT_EQ(1u, CountEntries());

  // This will make the cache empty. Note that the cache expects the num of
  // remaining entries should be greater than the specified purge unit.
  MayPurge(0u, 1u);
  EXPECT_EQ(0u, CountEntries());
}

TEST_F(PreflightCacheTest, CacheTimeout) {
  const std::string origin("null");
  const GURL url("http://www.test.com/A");
  const GURL other_url("http://www.test.com/B");

  EXPECT_EQ(0u, CountEntries());

  AppendEntry(origin, url);
  AppendEntry(origin, other_url);

  EXPECT_EQ(2u, CountEntries());

  // Cache entry should still be valid.
  EXPECT_TRUE(CheckEntryAndRefreshCache(origin, url));

  // Advance time by ten seconds.
  Advance(10);

  // Cache entry should now be expired.
  EXPECT_FALSE(CheckEntryAndRefreshCache(origin, url));

  EXPECT_EQ(1u, CountEntries());

  // Cache entry should be expired.
  EXPECT_FALSE(CheckEntryAndRefreshCache(origin, other_url));

  EXPECT_EQ(0u, CountEntries());
}

TEST_F(PreflightCacheTest, EstimateMemoryPressure) {
  const std::string origin1("origin1");
  const std::string origin2("origin2");
  const size_t entry_size = CreateEntry()->EstimateMemoryPressureInBytes();
  const GURL url1("http://www.test.com/pulstar");
  const GURL url2("http://www.test.com/blazingstar");

  EXPECT_EQ(0u, CountEntries());
  size_t expected_pressure = 0u;
  EXPECT_EQ(expected_pressure, EstimateMemoryPressure());

  AppendEntry(origin1, url1);
  expected_pressure += origin1.length() + url1.spec().length() + entry_size;
  EXPECT_EQ(expected_pressure, EstimateMemoryPressure());

  // Overwriting does not change the pressure.
  AppendEntry(origin1, url1);
  EXPECT_EQ(expected_pressure, EstimateMemoryPressure());

  // Add another entry.
  AppendEntry(origin1, url2);
  expected_pressure += origin1.length() + url2.spec().length() + entry_size;
  EXPECT_EQ(expected_pressure, EstimateMemoryPressure());

  // Expiring the cache entry should result in updating the memory pressure.
  Advance(10);
  EXPECT_FALSE(CheckEntryAndRefreshCache(origin1, url1));
  expected_pressure = origin1.length() + url2.spec().length() + entry_size;
  EXPECT_EQ(expected_pressure, EstimateMemoryPressure());

  // Add another entry that have the same memory pressure, then purge one.
  ASSERT_EQ(origin1.length(), origin2.length());
  AppendEntry(origin2, url2);
  EXPECT_EQ(expected_pressure * 2u, EstimateMemoryPressure());
  MayPurge(1u, 1u);
  EXPECT_EQ(expected_pressure, EstimateMemoryPressure());
}

}  // namespace

}  // namespace cors

}  // namespace network
