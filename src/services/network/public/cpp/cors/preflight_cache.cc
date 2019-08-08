// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cors/preflight_cache.h"

#include <iterator>

#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace network {

namespace cors {

namespace {
constexpr size_t kMaxCacheEntries = 1024u;
constexpr size_t kMaxKeyLength = 512u;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CacheMetric {
  kHitAndPass = 0,
  kHitAndFail = 1,
  kMiss = 2,
  kStale = 3,

  kMaxValue = kStale,
};

void ReportCacheMetric(CacheMetric metric) {
  UMA_HISTOGRAM_ENUMERATION("Net.Cors.PreflightCacheResult", metric);
}

}  // namespace

PreflightCache::PreflightCache() = default;
PreflightCache::~PreflightCache() = default;

void PreflightCache::AppendEntry(
    const std::string& origin,
    const GURL& url,
    std::unique_ptr<PreflightResult> preflight_result) {
  DCHECK(preflight_result);

  // Do not cache |preflight_result| if |url| is too long.
  const std::string& key = url.spec();
  if (key.length() >= kMaxKeyLength)
    return;

  // Since one new entry is always added below, let's purge one cache entry
  // if cache size is larger than kMaxCacheEntries - 1 so that the size to be
  // kMaxCacheEntries at maximum.
  MayPurge(kMaxCacheEntries - 1);

  cache_[origin][key] = std::move(preflight_result);
}

bool PreflightCache::CheckIfRequestCanSkipPreflight(
    const std::string& origin,
    const GURL& url,
    mojom::FetchCredentialsMode credentials_mode,
    const std::string& method,
    const net::HttpRequestHeaders& request_headers,
    bool is_revalidating) {
  // Either |origin| or |url| are not in cache.
  auto cache_per_origin = cache_.find(origin);
  if (cache_per_origin == cache_.end()) {
    ReportCacheMetric(CacheMetric::kMiss);
    return false;
  }

  auto cache_entry = cache_per_origin->second.find(url.spec());
  if (cache_entry == cache_per_origin->second.end()) {
    ReportCacheMetric(CacheMetric::kMiss);
    return false;
  }

  // Check if the entry is still valid.
  if (cache_entry->second->IsExpired()) {
    ReportCacheMetric(CacheMetric::kStale);
  } else {
    // Both |origin| and |url| are in cache. Check if the entry is sufficient to
    // skip CORS-preflight.
    if (cache_entry->second->EnsureAllowedRequest(
            credentials_mode, method, request_headers, is_revalidating)) {
      ReportCacheMetric(CacheMetric::kHitAndPass);
      return true;
    }
    ReportCacheMetric(CacheMetric::kHitAndFail);
  }

  // The cache entry is either stale or not sufficient. Remove the item from the
  // cache.
  cache_per_origin->second.erase(url.spec());
  if (cache_per_origin->second.empty())
    cache_.erase(cache_per_origin);

  return false;
}

size_t PreflightCache::ReportAndGatherSizeMetric() {
  size_t entries = CountEntries();
  UMA_HISTOGRAM_COUNTS_10000("Net.Cors.PreflightCacheEntries", entries);
  return entries;
}

size_t PreflightCache::CountOriginsForTesting() const {
  return cache_.size();
}

size_t PreflightCache::CountEntriesForTesting() const {
  return CountEntries();
}

void PreflightCache::MayPurgeForTesting(size_t max_entries) {
  MayPurge(max_entries);
}

size_t PreflightCache::CountEntries() const {
  size_t entries = 0;
  for (auto const& cache_per_origin : cache_)
    entries += cache_per_origin.second.size();
  return entries;
}

void PreflightCache::MayPurge(size_t max_entries) {
  if (CountEntries() <= max_entries)
    return;
  auto target_origin_cache = cache_.begin();
  std::advance(target_origin_cache, base::RandInt(0, cache_.size() - 1));
  if (target_origin_cache->second.size() == 1) {
    cache_.erase(target_origin_cache);
  } else {
    auto target_cache_entry = target_origin_cache->second.begin();
    std::advance(target_cache_entry,
                 base::RandInt(0, target_origin_cache->second.size() - 1));
    target_origin_cache->second.erase(target_cache_entry);
  }
}

}  // namespace cors

}  // namespace network
