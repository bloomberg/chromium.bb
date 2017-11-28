// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebCORSPreflightResultCache.h"

#include "base/test/simple_test_tick_clock.h"
#include "platform/network/HTTPHeaderMap.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/Time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace test {

class TestWebCORSPreflightResultCache : public WebCORSPreflightResultCache {
 public:
  TestWebCORSPreflightResultCache() = default;
  ~TestWebCORSPreflightResultCache() override = default;

  // Counts origins and urls in the cache. Thus with a single cache item, this
  // method would return 2. This is to tests whether we clean up the references
  // on all levels.
  int CacheSize() {
    int size = 0;

    for (auto const& origin : preflight_hash_map_) {
      size++;
      for (auto const& url : origin.second) {
        (void)url;  // Prevents unused compile error.
        size++;
      }
    }

    return size;
  }
};

}  // namespace test

class WebCORSPreflightResultCacheTest : public ::testing::Test {
 protected:
  std::unique_ptr<WebCORSPreflightResultCacheItem> CreateCacheItem(
      const AtomicString allow_methods,
      const AtomicString allow_headers,
      network::mojom::FetchCredentialsMode credentials_mode,
      const int max_age = -1) {
    HTTPHeaderMap response_header;

    if (!allow_methods.IsEmpty())
      response_header.Set("Access-Control-Allow-Methods", allow_methods);

    if (!allow_headers.IsEmpty())
      response_header.Set("Access-Control-Allow-Headers", allow_headers);

    if (max_age > -1) {
      response_header.Set("Access-Control-Max-Age",
                          AtomicString::Number(max_age));
    }

    WebString error_description;
    std::unique_ptr<WebCORSPreflightResultCacheItem> item =
        WebCORSPreflightResultCacheItem::Create(
            credentials_mode, response_header, error_description, clock());

    EXPECT_TRUE(item);

    return item;
  }

  base::SimpleTestTickClock* clock() { return &clock_; }

  // This is by no means a robust parser and works only for the headers strings
  // used in this tests.
  HTTPHeaderMap ParseHeaderString(std::string headers) {
    HTTPHeaderMap header_map;
    std::stringstream stream;
    stream.str(headers);
    std::string line;
    while (std::getline(stream, line, ';')) {
      int colon = line.find_first_of(':');

      header_map.Add(
          AtomicString::FromUTF8(line.substr(0, colon).data()),
          AtomicString::FromUTF8(line.substr(colon + 1, line.size()).data()));
    }

    return header_map;
  }

 private:
  base::SimpleTestTickClock clock_;
};

TEST_F(WebCORSPreflightResultCacheTest, CacheTimeout) {
  WebString origin("null");
  WebURL url = URLTestHelpers::ToKURL("http://www.test.com/A");
  WebURL other_url = URLTestHelpers::ToKURL("http://www.test.com/B");

  test::TestWebCORSPreflightResultCache cache;

  // Cache should be empty:
  EXPECT_EQ(0, cache.CacheSize());

  cache.AppendEntry(
      origin, url,
      CreateCacheItem("POST", "",
                      network::mojom::FetchCredentialsMode::kInclude, 5));
  cache.AppendEntry(
      origin, other_url,
      CreateCacheItem("POST", "",
                      network::mojom::FetchCredentialsMode::kInclude, 5));

  // Cache size should be 3 (counting origins and urls separately):
  EXPECT_EQ(3, cache.CacheSize());

  // Cache entry should still be valid:
  EXPECT_TRUE(cache.CanSkipPreflight(
      origin, url, network::mojom::FetchCredentialsMode::kInclude, "POST",
      HTTPHeaderMap()));

  // Advance time by ten seconds:
  clock()->Advance(TimeDelta::FromSeconds(10));

  // Cache entry should now be expired:
  EXPECT_FALSE(cache.CanSkipPreflight(
      origin, url, network::mojom::FetchCredentialsMode::kInclude, "POST",
      HTTPHeaderMap()));

  // Cache size should be 2, with the expired entry removed by call to
  // CanSkipPreflight() but one origin and one url still in the cache:
  EXPECT_EQ(2, cache.CacheSize());

  // Cache entry should be expired:
  EXPECT_FALSE(cache.CanSkipPreflight(
      origin, other_url, network::mojom::FetchCredentialsMode::kInclude, "POST",
      HTTPHeaderMap()));

  // Cache size should be 0, with the expired entry removed by call to
  // CanSkipPreflight():
  EXPECT_EQ(0, cache.CacheSize());
}

TEST_F(WebCORSPreflightResultCacheTest, CacheSize) {
  WebString origin("null");
  WebString other_origin("http://www.other.com:80");
  WebURL url = URLTestHelpers::ToKURL("http://www.test.com/A");
  WebURL other_url = URLTestHelpers::ToKURL("http://www.test.com/B");

  test::TestWebCORSPreflightResultCache cache;

  // Cache should be empty:
  EXPECT_EQ(0, cache.CacheSize());

  cache.AppendEntry(
      origin, url,
      CreateCacheItem("POST", "",
                      network::mojom::FetchCredentialsMode::kInclude));

  // Cache size should be 2 (counting origins and urls separately):
  EXPECT_EQ(2, cache.CacheSize());

  cache.AppendEntry(
      origin, other_url,
      CreateCacheItem("POST", "",
                      network::mojom::FetchCredentialsMode::kInclude));

  // Cache size should now be 3 (1 origin, 2 urls):
  EXPECT_EQ(3, cache.CacheSize());

  cache.AppendEntry(
      other_origin, url,
      CreateCacheItem("POST", "",
                      network::mojom::FetchCredentialsMode::kInclude));
  // Cache size should now be 4 (4 origin, 3 urls):
  EXPECT_EQ(5, cache.CacheSize());
}

TEST_F(WebCORSPreflightResultCacheTest, CanSkipPreflight) {
  const struct {
    const AtomicString allow_methods;
    const AtomicString allow_headers;
    const network::mojom::FetchCredentialsMode cache_credentials_mode;

    const AtomicString request_method;
    const std::string request_headers;
    const network::mojom::FetchCredentialsMode request_credentials_mode;

    bool can_skip_preflight;
  } tests[] = {
      // Different methods:
      {"OPTIONS", "", network::mojom::FetchCredentialsMode::kOmit, "OPTIONS",
       "", network::mojom::FetchCredentialsMode::kOmit, true},
      {"GET", "", network::mojom::FetchCredentialsMode::kOmit, "GET", "",
       network::mojom::FetchCredentialsMode::kOmit, true},
      {"HEAD", "", network::mojom::FetchCredentialsMode::kOmit, "HEAD", "",
       network::mojom::FetchCredentialsMode::kOmit, true},
      {"POST", "", network::mojom::FetchCredentialsMode::kOmit, "POST", "",
       network::mojom::FetchCredentialsMode::kOmit, true},
      {"PUT", "", network::mojom::FetchCredentialsMode::kOmit, "PUT", "",
       network::mojom::FetchCredentialsMode::kOmit, true},
      {"DELETE", "", network::mojom::FetchCredentialsMode::kOmit, "DELETE", "",
       network::mojom::FetchCredentialsMode::kOmit, true},

      // Cache allowing multiple methods:
      {"GET, PUT, DELETE", "", network::mojom::FetchCredentialsMode::kOmit,
       "GET", "", network::mojom::FetchCredentialsMode::kOmit, true},
      {"GET, PUT, DELETE", "", network::mojom::FetchCredentialsMode::kOmit,
       "PUT", "", network::mojom::FetchCredentialsMode::kOmit, true},
      {"GET, PUT, DELETE", "", network::mojom::FetchCredentialsMode::kOmit,
       "DELETE", "", network::mojom::FetchCredentialsMode::kOmit, true},

      // Methods not in cached preflight response:
      {"GET", "", network::mojom::FetchCredentialsMode::kOmit, "PUT", "",
       network::mojom::FetchCredentialsMode::kOmit, false},
      {"GET, POST, DELETE", "", network::mojom::FetchCredentialsMode::kOmit,
       "PUT", "", network::mojom::FetchCredentialsMode::kOmit, false},

      // Allowed headers:
      {"GET", "X-MY-HEADER", network::mojom::FetchCredentialsMode::kOmit, "GET",
       "X-MY-HEADER:t", network::mojom::FetchCredentialsMode::kOmit, true},
      {"GET", "X-MY-HEADER, Y-MY-HEADER",
       network::mojom::FetchCredentialsMode::kOmit, "GET",
       "X-MY-HEADER:t;Y-MY-HEADER:t",
       network::mojom::FetchCredentialsMode::kOmit, true},
      {"GET", "x-my-header, Y-MY-HEADER",
       network::mojom::FetchCredentialsMode::kOmit, "GET",
       "X-MY-HEADER:t;y-my-header:t",
       network::mojom::FetchCredentialsMode::kOmit, true},

      // Requested headers not in cached preflight response:
      {"GET", "", network::mojom::FetchCredentialsMode::kOmit, "GET",
       "X-MY-HEADER:t", network::mojom::FetchCredentialsMode::kOmit, false},
      {"GET", "X-SOME-OTHER-HEADER",
       network::mojom::FetchCredentialsMode::kOmit, "GET", "X-MY-HEADER:t",
       network::mojom::FetchCredentialsMode::kOmit, false},
      {"GET", "X-MY-HEADER", network::mojom::FetchCredentialsMode::kOmit, "GET",
       "X-MY-HEADER:t;Y-MY-HEADER:t",
       network::mojom::FetchCredentialsMode::kOmit, false},

      // Different credential modes:
      {"GET", "", network::mojom::FetchCredentialsMode::kInclude, "GET", "",
       network::mojom::FetchCredentialsMode::kOmit, true},
      {"GET", "", network::mojom::FetchCredentialsMode::kInclude, "GET", "",
       network::mojom::FetchCredentialsMode::kInclude, true},

      // Credential mode mismatch:
      {"GET", "", network::mojom::FetchCredentialsMode::kOmit, "GET", "",
       network::mojom::FetchCredentialsMode::kInclude, false},
      {"GET", "", network::mojom::FetchCredentialsMode::kOmit, "GET", "",
       network::mojom::FetchCredentialsMode::kInclude, false},
  };

  for (const auto& test : tests) {
    SCOPED_TRACE(
        ::testing::Message()
        << "allow_methods: [" << test.allow_methods << "], allow_headers: ["
        << test.allow_headers << "], request_method: [" << test.request_method
        << "], request_headers: [" << test.request_headers
        << "] expect CanSkipPreflight() to return " << test.can_skip_preflight);

    WebString origin("null");
    WebURL url = URLTestHelpers::ToKURL("http://www.test.com/");

    std::unique_ptr<WebCORSPreflightResultCacheItem> item = CreateCacheItem(
        test.allow_methods, test.allow_headers, test.cache_credentials_mode);

    EXPECT_TRUE(item);

    test::TestWebCORSPreflightResultCache cache;

    cache.AppendEntry(origin, url, std::move(item));

    EXPECT_EQ(cache.CanSkipPreflight(origin, url, test.request_credentials_mode,
                                     test.request_method,
                                     ParseHeaderString(test.request_headers)),
              test.can_skip_preflight);
  }
}

}  // namespace blink
