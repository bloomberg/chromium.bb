// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/appcache/manifest_parser.h"

namespace appcache {

class ManifestParserTest : public testing::Test {
};

TEST(ManifestParserTest, NoData) {
  GURL url;
  Manifest manifest;
  EXPECT_FALSE(ParseManifest(url, "", 0, manifest));
  EXPECT_FALSE(ParseManifest(url, "CACHE MANIFEST\r", 0, manifest));  // 0 len
}

TEST(ManifestParserTest, CheckSignature) {
  GURL url;
  Manifest manifest;

  const std::string kBadSignatures[] = {
    "foo",
    "CACHE MANIFEST;V2\r",          // not followed by whitespace
    "CACHE MANIFEST#bad\r",         // no whitespace before comment
    "cache manifest ",              // wrong case
    "#CACHE MANIFEST\r",            // comment
    "xCACHE MANIFEST\n",            // bad first char
    " CACHE MANIFEST\r",            // begins with whitespace
    "\xEF\xBE\xBF" "CACHE MANIFEST\r",  // bad UTF-8 BOM value
  };

  for (size_t i = 0; i < arraysize(kBadSignatures); ++i) {
    const std::string bad = kBadSignatures[i];
    EXPECT_FALSE(ParseManifest(url, bad.c_str(), bad.length(), manifest));
  }

  const std::string kGoodSignatures[] = {
    "CACHE MANIFEST",
    "CACHE MANIFEST ",
    "CACHE MANIFEST\r",
    "CACHE MANIFEST\n",
    "CACHE MANIFEST\r\n",
    "CACHE MANIFEST\t# ignore me\r",
    "CACHE MANIFEST ignore\r\n",
    "\xEF\xBB\xBF" "CACHE MANIFEST \r\n",   // BOM present
  };

  for (size_t i = 0; i < arraysize(kGoodSignatures); ++i) {
    const std::string good = kGoodSignatures[i];
    EXPECT_TRUE(ParseManifest(url, good.c_str(), good.length(), manifest));
  }
}

TEST(ManifestParserTest, NoManifestUrl) {
  Manifest manifest;
  const std::string kData("CACHE MANIFEST\r"
    "relative/tobase.com\r"
    "http://absolute.com/addme.com");
  const GURL kUrl;
  EXPECT_TRUE(ParseManifest(kUrl, kData.c_str(), kData.length(), manifest));
  EXPECT_TRUE(manifest.explicit_urls.empty());
  EXPECT_TRUE(manifest.fallback_namespaces.empty());
  EXPECT_TRUE(manifest.online_whitelist_namespaces.empty());
  EXPECT_FALSE(manifest.online_whitelist_all);
}

TEST(ManifestParserTest, ExplicitUrls) {
  Manifest manifest;
  const GURL kUrl("http://www.foo.com");
  const std::string kData("CACHE MANIFEST\r"
    "relative/one\r"
    "# some comment\r"
    "http://www.foo.com/two#strip\r\n"
    "NETWORK:\r"
    "  \t CACHE:\r"
    "HTTP://www.diff.com/three\r"
    "FALLBACK:\r"
    " \t # another comment with leading whitespace\n"
    "IGNORE:\r"
    "http://www.foo.com/ignore\r"
    "CACHE: \r"
    "garbage:#!@\r"
    "https://www.foo.com/diffscheme \t \r"
    "  \t relative/four#stripme\n\r"
    "*\r");

  EXPECT_TRUE(ParseManifest(kUrl, kData.c_str(), kData.length(), manifest));
  EXPECT_TRUE(manifest.fallback_namespaces.empty());
  EXPECT_TRUE(manifest.online_whitelist_namespaces.empty());
  EXPECT_FALSE(manifest.online_whitelist_all);

  base::hash_set<std::string> urls = manifest.explicit_urls;
  const size_t kExpected = 5;
  ASSERT_EQ(kExpected, urls.size());
  EXPECT_TRUE(urls.find("http://www.foo.com/relative/one") != urls.end());
  EXPECT_TRUE(urls.find("http://www.foo.com/two") != urls.end());
  EXPECT_TRUE(urls.find("http://www.diff.com/three") != urls.end());
  EXPECT_TRUE(urls.find("http://www.foo.com/relative/four") != urls.end());

  // Wildcard is treated as a relative URL in explicit section.
  EXPECT_TRUE(urls.find("http://www.foo.com/*") != urls.end());
}

TEST(ManifestParserTest, WhitelistUrls) {
  Manifest manifest;
  const GURL kUrl("http://www.bar.com");
  const std::string kData("CACHE MANIFEST\r"
    "NETWORK:\r"
    "relative/one\r"
    "# a comment\r"
    "http://www.bar.com/two\r"
    "HTTP://www.diff.com/three#strip\n\r"
    "FALLBACK:\r"
    "garbage\r"
    "UNKNOWN:\r"
    "http://www.bar.com/ignore\r"
    "CACHE:\r"
    "NETWORK:\r"
    "https://www.wrongscheme.com\n"
    "relative/four#stripref \t \r"
    "http://www.five.com\r\n"
    "*foo\r");

  EXPECT_TRUE(ParseManifest(kUrl, kData.c_str(), kData.length(), manifest));
  EXPECT_TRUE(manifest.explicit_urls.empty());
  EXPECT_TRUE(manifest.fallback_namespaces.empty());
  EXPECT_FALSE(manifest.online_whitelist_all);

  std::vector<GURL> online = manifest.online_whitelist_namespaces;
  const size_t kExpected = 6;
  ASSERT_EQ(kExpected, online.size());
  EXPECT_EQ(GURL("http://www.bar.com/relative/one"), online[0]);
  EXPECT_EQ(GURL("http://www.bar.com/two"), online[1]);
  EXPECT_EQ(GURL("http://www.diff.com/three"), online[2]);
  EXPECT_EQ(GURL("http://www.bar.com/relative/four"), online[3]);
  EXPECT_EQ(GURL("http://www.five.com"), online[4]);
  EXPECT_EQ(GURL("http://www.bar.com/*foo"), online[5]);
}

TEST(ManifestParserTest, FallbackUrls) {
  Manifest manifest;
  const GURL kUrl("http://glorp.com");
  const std::string kData("CACHE MANIFEST\r"
    "# a comment\r"
    "CACHE:\r"
    "NETWORK:\r"
    "UNKNOWN:\r"
    "FALLBACK:\r"
    "relative/one \t \t http://glorp.com/onefb  \t \r"
    "*\r"
    "https://glorp.com/wrong http://glorp.com/wrongfb\r"
    "http://glorp.com/two#strip relative/twofb\r"
    "HTTP://glorp.com/three relative/threefb#strip\n"
    "http://glorp.com/three http://glorp.com/three-dup\r"
    "http://glorp.com/solo \t \r\n"
    "http://diff.com/ignore http://glorp.com/wronghost\r"
    "http://glorp.com/wronghost http://diff.com/ohwell\r"
    "relative/badscheme ftp://glorp.com/ignored\r"
    "garbage\r\n"
    "CACHE:\r"
    "# only fallback urls in this test\r"
    "FALLBACK:\n"
    "relative/four#strip relative/fourfb#strip\r"
    "http://www.glorp.com/notsame relative/skipped\r");

  EXPECT_TRUE(ParseManifest(kUrl, kData.c_str(), kData.length(), manifest));
  EXPECT_TRUE(manifest.explicit_urls.empty());
  EXPECT_TRUE(manifest.online_whitelist_namespaces.empty());
  EXPECT_FALSE(manifest.online_whitelist_all);

  std::vector<FallbackNamespace> fallbacks = manifest.fallback_namespaces;
  const size_t kExpected = 5;
  ASSERT_EQ(kExpected, fallbacks.size());
  EXPECT_EQ(GURL("http://glorp.com/relative/one"),
            fallbacks[0].first);
  EXPECT_EQ(GURL("http://glorp.com/onefb"),
            fallbacks[0].second);
  EXPECT_EQ(GURL("http://glorp.com/two"),
            fallbacks[1].first);
  EXPECT_EQ(GURL("http://glorp.com/relative/twofb"),
            fallbacks[1].second);
  EXPECT_EQ(GURL("http://glorp.com/three"),
            fallbacks[2].first);
  EXPECT_EQ(GURL("http://glorp.com/relative/threefb"),
            fallbacks[2].second);
  EXPECT_EQ(GURL("http://glorp.com/three"),       // duplicates are stored
            fallbacks[3].first);
  EXPECT_EQ(GURL("http://glorp.com/three-dup"),
            fallbacks[3].second);
  EXPECT_EQ(GURL("http://glorp.com/relative/four"),
            fallbacks[4].first);
  EXPECT_EQ(GURL("http://glorp.com/relative/fourfb"),
            fallbacks[4].second);
}

TEST(ManifestParserTest, FallbackUrlsWithPort) {
  Manifest manifest;
  const GURL kUrl("http://www.portme.com:1234");
  const std::string kData("CACHE MANIFEST\r"
    "FALLBACK:\r"
    "http://www.portme.com:1234/one relative/onefb\r"
    "HTTP://www.portme.com:9876/wrong http://www.portme.com:1234/ignore\r"
    "http://www.portme.com:1234/stillwrong http://www.portme.com:42/boo\r"
    "relative/two relative/twofb\r"
    "http://www.portme.com:1234/three HTTP://www.portme.com:1234/threefb\r"
    "http://www.portme.com/noport http://www.portme.com:1234/skipped\r"
    "http://www.portme.com:1234/skipme http://www.portme.com/noport\r");

  EXPECT_TRUE(ParseManifest(kUrl, kData.c_str(), kData.length(), manifest));
  EXPECT_TRUE(manifest.explicit_urls.empty());
  EXPECT_TRUE(manifest.online_whitelist_namespaces.empty());
  EXPECT_FALSE(manifest.online_whitelist_all);

  std::vector<FallbackNamespace> fallbacks = manifest.fallback_namespaces;
  const size_t kExpected = 3;
  ASSERT_EQ(kExpected, fallbacks.size());
  EXPECT_EQ(GURL("http://www.portme.com:1234/one"),
            fallbacks[0].first);
  EXPECT_EQ(GURL("http://www.portme.com:1234/relative/onefb"),
            fallbacks[0].second);
  EXPECT_EQ(GURL("http://www.portme.com:1234/relative/two"),
            fallbacks[1].first);
  EXPECT_EQ(GURL("http://www.portme.com:1234/relative/twofb"),
            fallbacks[1].second);
  EXPECT_EQ(GURL("http://www.portme.com:1234/three"),
            fallbacks[2].first);
  EXPECT_EQ(GURL("http://www.portme.com:1234/threefb"),
            fallbacks[2].second);
}

TEST(ManifestParserTest, ComboUrls) {
  Manifest manifest;
  const GURL kUrl("http://combo.com:42");
  const std::string kData("CACHE MANIFEST\r"
    "relative/explicit-1\r"
    "# some comment\r"
    "http://combo.com:99/explicit-2#strip\r"
    "NETWORK:\r"
    "http://combo.com/whitelist-1\r"
    "HTTP://www.diff.com/whitelist-2#strip\r"
    "*\r"
    "CACHE:\n\r"
    "http://www.diff.com/explicit-3\r"
    "FALLBACK:\r"
    "http://combo.com:42/fallback-1 http://combo.com:42/fallback-1b\r"
    "relative/fallback-2 relative/fallback-2b\r"
    "UNKNOWN:\r\n"
    "http://combo.com/ignoreme\r"
    "relative/still-ignored\r"
    "NETWORK:\r\n"
    "relative/whitelist-3#strip\r"
    "http://combo.com:99/whitelist-4\r");
  EXPECT_TRUE(ParseManifest(kUrl, kData.c_str(), kData.length(), manifest));
  EXPECT_TRUE(manifest.online_whitelist_all);

  base::hash_set<std::string> urls = manifest.explicit_urls;
  size_t expected = 3;
  ASSERT_EQ(expected, urls.size());
  EXPECT_TRUE(urls.find("http://combo.com:42/relative/explicit-1") !=
              urls.end());
  EXPECT_TRUE(urls.find("http://combo.com:99/explicit-2") != urls.end());
  EXPECT_TRUE(urls.find("http://www.diff.com/explicit-3") != urls.end());

  std::vector<GURL> online = manifest.online_whitelist_namespaces;
  expected = 4;
  ASSERT_EQ(expected, online.size());
  EXPECT_EQ(GURL("http://combo.com/whitelist-1"), online[0]);
  EXPECT_EQ(GURL("http://www.diff.com/whitelist-2"), online[1]);
  EXPECT_EQ(GURL("http://combo.com:42/relative/whitelist-3"), online[2]);
  EXPECT_EQ(GURL("http://combo.com:99/whitelist-4"), online[3]);

  std::vector<FallbackNamespace> fallbacks = manifest.fallback_namespaces;
  expected = 2;
  ASSERT_EQ(expected, fallbacks.size());
  EXPECT_EQ(GURL("http://combo.com:42/fallback-1"),
            fallbacks[0].first);
  EXPECT_EQ(GURL("http://combo.com:42/fallback-1b"),
            fallbacks[0].second);
  EXPECT_EQ(GURL("http://combo.com:42/relative/fallback-2"),
            fallbacks[1].first);
  EXPECT_EQ(GURL("http://combo.com:42/relative/fallback-2b"),
            fallbacks[1].second);
}

TEST(ManifestParserTest, UnusualUtf8) {
  Manifest manifest;
  const GURL kUrl("http://bad.com");
  const std::string kData("CACHE MANIFEST\r"
    "\xC0" "invalidutf8\r"
    "nonbmp" "\xF1\x84\xAB\xBC\r");
  EXPECT_TRUE(ParseManifest(kUrl, kData.c_str(), kData.length(), manifest));

  base::hash_set<std::string> urls = manifest.explicit_urls;
  EXPECT_TRUE(urls.find("http://bad.com/%EF%BF%BDinvalidutf8") != urls.end());
  EXPECT_TRUE(urls.find("http://bad.com/nonbmp%F1%84%AB%BC") != urls.end());
}

TEST(ManifestParserTest, IgnoreAfterSpace) {
  Manifest manifest;
  const GURL kUrl("http://smorg.borg");
  const std::string kData(
    "CACHE MANIFEST\r"
    "resource.txt this stuff after the white space should be ignored\r");
  EXPECT_TRUE(ParseManifest(kUrl, kData.c_str(), kData.length(), manifest));

  base::hash_set<std::string> urls = manifest.explicit_urls;
  EXPECT_TRUE(urls.find("http://smorg.borg/resource.txt") != urls.end());
}

TEST(ManifestParserTest, DifferentOriginUrlWithSecureScheme) {
  Manifest manifest;
  const GURL kUrl("https://www.foo.com");
  const std::string kData("CACHE MANIFEST\r"
    "CACHE: \r"
    "relative/secureschemesameorigin\r"
    "https://www.foo.com/secureschemesameorigin\r"
    "http://www.xyz.com/secureschemedifforigin\r"
    "https://www.xyz.com/secureschemedifforigin\r");

  EXPECT_TRUE(ParseManifest(kUrl, kData.c_str(), kData.length(), manifest));
  EXPECT_TRUE(manifest.fallback_namespaces.empty());
  EXPECT_TRUE(manifest.online_whitelist_namespaces.empty());

  base::hash_set<std::string> urls = manifest.explicit_urls;
  const size_t kExpected = 3;
  ASSERT_EQ(kExpected, urls.size());
  EXPECT_TRUE(urls.find("https://www.foo.com/relative/secureschemesameorigin")
      != urls.end());
  EXPECT_TRUE(urls.find("https://www.foo.com/secureschemesameorigin") !=
      urls.end());
  EXPECT_FALSE(urls.find("http://www.xyz.com/secureschemedifforigin") !=
      urls.end());
  EXPECT_TRUE(urls.find("https://www.xyz.com/secureschemedifforigin") !=
      urls.end());
}

}  // namespace appcache

