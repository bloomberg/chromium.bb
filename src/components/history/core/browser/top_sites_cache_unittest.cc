// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/top_sites_cache.h"

#include <stddef.h>

#include <set>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history {

namespace {

class TopSitesCacheTest : public testing::Test {
 public:
  TopSitesCacheTest() {
  }

 protected:
  // Initializes |top_sites_| on |spec|, which is a list of URL strings with
  // optional indents: indentated URLs redirect to the last non-indented URL.
  // Titles are assigned as "Title 1", "Title 2", etc., in the order of
  // appearance. See |kTopSitesSpecBasic| for an example. This function does not
  // update |cache_| so you can manipulate |top_sites_| before you update it.
  void BuildTopSites(const char** spec, size_t size);

  // Initializes |top_sites_| and |cache_| based on |spec|.
  void InitTopSiteCache(const char** spec, size_t size);

  MostVisitedURLList top_sites_;
  TopSitesCache cache_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TopSitesCacheTest);
};

void TopSitesCacheTest::BuildTopSites(const char** spec, size_t size) {
  std::set<std::string> urls_seen;
  for (size_t i = 0; i < size; ++i) {
    const char* spec_item = spec[i];
    while (*spec_item && *spec_item == ' ')  // Eat indent.
      ++spec_item;
    if (urls_seen.find(spec_item) != urls_seen.end())
      NOTREACHED() << "Duplicate URL found: " << spec_item;
    urls_seen.insert(spec_item);
    if (spec_item == spec[i]) {  // No indent: add new MostVisitedURL.
      base::string16 title(base::ASCIIToUTF16("Title ") +
                           base::NumberToString16(top_sites_.size() + 1));
      top_sites_.push_back(MostVisitedURL(GURL(spec_item), title));
    }
    ASSERT_TRUE(!top_sites_.empty());
    // Set up redirect to canonical URL. Canonical URL redirects to itself, too.
    top_sites_.back().redirects.push_back(GURL(spec_item));
  }
}

void TopSitesCacheTest::InitTopSiteCache(const char** spec, size_t size) {
  BuildTopSites(spec, size);
  cache_.SetTopSites(top_sites_);
}

const char* kTopSitesSpecBasic[] = {
  "http://www.google.com",
  "  http://www.gogle.com",  // Redirects.
  "  http://www.gooogle.com",  // Redirects.
  "http://www.youtube.com/a/b",
  "  http://www.youtube.com/a/b?test=1",  // Redirects.
  "https://www.google.com/",
  "  https://www.gogle.com",  // Redirects.
  "http://www.example.com:3141/",
};

TEST_F(TopSitesCacheTest, GetCanonicalURL) {
  InitTopSiteCache(kTopSitesSpecBasic, base::size(kTopSitesSpecBasic));
  struct {
    const char* expected;
    const char* query;
  } test_cases[] = {
    // Already is canonical: redirects.
    {"http://www.google.com/", "http://www.google.com"},
    // Exact match with stored URL: redirects.
    {"http://www.google.com/", "http://www.gooogle.com"},
    // Recognizes despite trailing "/": redirects
    {"http://www.google.com/", "http://www.gooogle.com/"},
    // Exact match with URL with query: redirects.
    {"http://www.youtube.com/a/b", "http://www.youtube.com/a/b?test=1"},
    // No match with URL with query: as-is.
    {"http://www.youtube.com/a/b?test", "http://www.youtube.com/a/b?test"},
    // Never-seen-before URL: as-is.
    {"http://maps.google.com/", "http://maps.google.com/"},
    // Changing port number, does not match: as-is.
    {"http://www.example.com:1234/", "http://www.example.com:1234"},
    // Smart enough to know that port 80 is HTTP: redirects.
    {"http://www.google.com/", "http://www.gooogle.com:80"},
    // Prefix should not work: as-is.
    {"http://www.youtube.com/a", "http://www.youtube.com/a"},
  };
  for (size_t i = 0; i < base::size(test_cases); ++i) {
    std::string expected(test_cases[i].expected);
    std::string query(test_cases[i].query);
    EXPECT_EQ(expected, cache_.GetCanonicalURL(GURL(query)).spec())
      << " for test_case[" << i << "]";
  }
}

TEST_F(TopSitesCacheTest, IsKnownUrl) {
  InitTopSiteCache(kTopSitesSpecBasic, base::size(kTopSitesSpecBasic));
  // Matches.
  EXPECT_TRUE(cache_.IsKnownURL(GURL("http://www.google.com")));
  EXPECT_TRUE(cache_.IsKnownURL(GURL("http://www.gooogle.com")));
  EXPECT_TRUE(cache_.IsKnownURL(GURL("http://www.google.com/")));

  // Non-matches.
  EXPECT_FALSE(cache_.IsKnownURL(GURL("http://www.google.com?")));
  EXPECT_FALSE(cache_.IsKnownURL(GURL("http://www.google.net")));
  EXPECT_FALSE(cache_.IsKnownURL(GURL("http://www.google.com/stuff")));
  EXPECT_FALSE(cache_.IsKnownURL(GURL("https://www.gooogle.com")));
  EXPECT_FALSE(cache_.IsKnownURL(GURL("http://www.youtube.com/a")));
}

}  // namespace

}  // namespace history
