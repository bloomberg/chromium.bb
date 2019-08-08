// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/url_util.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace policy {
namespace url_util {

namespace {

GURL GetEmbeddedURL(const std::string& url) {
  return policy::url_util::GetEmbeddedURL(GURL(url));
}

}  // namespace

TEST(URLUtilTest, Normalize) {
  // Username is cleared.
  EXPECT_EQ(Normalize(GURL("http://dino@example/foo")),
            GURL("http://example/foo"));

  // Username and password are cleared.
  EXPECT_EQ(Normalize(GURL("http://dino:hunter2@example/")),
            GURL("http://example/"));

  // Query string is cleared.
  EXPECT_EQ(Normalize(GURL("http://example.com/foo?widgetId=42")),
            GURL("http://example.com/foo"));
  EXPECT_EQ(Normalize(GURL("https://example.com/?widgetId=42&frobinate=true")),
            GURL("https://example.com/"));

  // Ref is cleared.
  EXPECT_EQ(Normalize(GURL("http://example.com/foo#widgetSection")),
            GURL("http://example.com/foo"));

  // Port is NOT cleared.
  EXPECT_EQ(Normalize(GURL("http://example.com:443/")),
            GURL("http://example.com:443/"));

  // All together now.
  EXPECT_EQ(
      Normalize(GURL("https://dino:hunter2@example.com:443/foo?widgetId=42")),
      GURL("https://example.com:443/foo"));
}

TEST(URLUtilTest, GetEmbeddedURLAmpCache) {
  // Base case.
  EXPECT_EQ(GURL("http://example.com"),
            GetEmbeddedURL("https://cdn.ampproject.org/c/example.com"));
  // "s/" means "use https".
  EXPECT_EQ(GURL("https://example.com"),
            GetEmbeddedURL("https://cdn.ampproject.org/c/s/example.com"));
  // With path and query. Fragment is not extracted.
  EXPECT_EQ(GURL("https://example.com/path/to/file.html?q=asdf"),
            GetEmbeddedURL("https://cdn.ampproject.org/c/s/example.com/path/to/"
                           "file.html?q=asdf#baz"));
  // Publish subdomain can be included but doesn't affect embedded URL.
  EXPECT_EQ(
      GURL("http://example.com"),
      GetEmbeddedURL("https://example-com.cdn.ampproject.org/c/example.com"));
  EXPECT_EQ(
      GURL("http://example.com"),
      GetEmbeddedURL("https://example-org.cdn.ampproject.org/c/example.com"));

  // Different host is not supported.
  EXPECT_EQ(GURL(), GetEmbeddedURL("https://www.ampproject.org/c/example.com"));
  // Different TLD is not supported.
  EXPECT_EQ(GURL(), GetEmbeddedURL("https://cdn.ampproject.com/c/example.com"));
  // Content type ("c/") is missing.
  EXPECT_EQ(GURL(), GetEmbeddedURL("https://cdn.ampproject.org/example.com"));
  // Content type is mis-formatted, must be a single character.
  EXPECT_EQ(GURL(),
            GetEmbeddedURL("https://cdn.ampproject.org/cd/example.com"));
}

TEST(URLUtilTest, GetEmbeddedURLGoogleAmpViewer) {
  // Base case.
  EXPECT_EQ(GURL("http://example.com"),
            GetEmbeddedURL("https://www.google.com/amp/example.com"));
  // "s/" means "use https".
  EXPECT_EQ(GURL("https://example.com"),
            GetEmbeddedURL("https://www.google.com/amp/s/example.com"));
  // Different Google TLDs are supported.
  EXPECT_EQ(GURL("http://example.com"),
            GetEmbeddedURL("https://www.google.de/amp/example.com"));
  EXPECT_EQ(GURL("http://example.com"),
            GetEmbeddedURL("https://www.google.co.uk/amp/example.com"));
  // With path.
  EXPECT_EQ(GURL("http://example.com/path"),
            GetEmbeddedURL("https://www.google.com/amp/example.com/path"));
  // Query is *not* part of the embedded URL.
  EXPECT_EQ(
      GURL("http://example.com/path"),
      GetEmbeddedURL("https://www.google.com/amp/example.com/path?q=baz"));
  // Query and fragment in percent-encoded form *are* part of the embedded URL.
  EXPECT_EQ(
      GURL("http://example.com/path?q=foo#bar"),
      GetEmbeddedURL(
          "https://www.google.com/amp/example.com/path%3fq=foo%23bar?q=baz"));
  // "/" may also be percent-encoded.
  EXPECT_EQ(GURL("http://example.com/path?q=foo#bar"),
            GetEmbeddedURL("https://www.google.com/amp/"
                           "example.com%2fpath%3fq=foo%23bar?q=baz"));

  // Missing "amp/".
  EXPECT_EQ(GURL(), GetEmbeddedURL("https://www.google.com/example.com"));
  // Path component before the "amp/".
  EXPECT_EQ(GURL(),
            GetEmbeddedURL("https://www.google.com/foo/amp/example.com"));
  // Different host.
  EXPECT_EQ(GURL(), GetEmbeddedURL("https://www.other.com/amp/example.com"));
  // Different subdomain.
  EXPECT_EQ(GURL(), GetEmbeddedURL("https://mail.google.com/amp/example.com"));
  // Invalid TLD.
  EXPECT_EQ(GURL(), GetEmbeddedURL("https://www.google.nope/amp/example.com"));

  // Valid TLD that is not considered safe to display to the user by
  // UnescapeURLComponent(). Note that when UTF-8 characters appear in a domain
  // name, as is the case here, they're replaced by equivalent punycode by the
  // GURL constructor.
  EXPECT_EQ(GURL("http://www.xn--iv8h.com/"),
            GetEmbeddedURL("https://www.google.com/amp/www.%F0%9F%94%8F.com/"));
  // Invalid UTF-8 characters.
  EXPECT_EQ(GURL("http://example.com/%81%82%83"),
            GetEmbeddedURL("https://www.google.com/amp/example.com/%81%82%83"));
}

TEST(URLUtilTest, GetEmbeddedURLGoogleWebCache) {
  // Base case.
  EXPECT_EQ(GURL("http://example.com"),
            GetEmbeddedURL("https://webcache.googleusercontent.com/"
                           "search?q=cache:ABCDEFGHI-JK:example.com/"));
  // With search query.
  EXPECT_EQ(
      GURL("http://example.com"),
      GetEmbeddedURL("https://webcache.googleusercontent.com/"
                     "search?q=cache:ABCDEFGHI-JK:example.com/+search_query"));
  // Without fingerprint.
  EXPECT_EQ(GURL("http://example.com"),
            GetEmbeddedURL("https://webcache.googleusercontent.com/"
                           "search?q=cache:example.com/"));
  // With search query, without fingerprint.
  EXPECT_EQ(GURL("http://example.com"),
            GetEmbeddedURL("https://webcache.googleusercontent.com/"
                           "search?q=cache:example.com/+search_query"));
  // Query params other than "q=" don't matter.
  EXPECT_EQ(GURL("http://example.com"),
            GetEmbeddedURL("https://webcache.googleusercontent.com/"
                           "search?a=b&q=cache:example.com/&c=d"));
  // With scheme.
  EXPECT_EQ(GURL("http://example.com"),
            GetEmbeddedURL("https://webcache.googleusercontent.com/"
                           "search?q=cache:http://example.com/"));
  // Preserve https.
  EXPECT_EQ(GURL("https://example.com"),
            GetEmbeddedURL("https://webcache.googleusercontent.com/"
                           "search?q=cache:https://example.com/"));

  // Wrong host.
  EXPECT_EQ(GURL(), GetEmbeddedURL("https://www.googleusercontent.com/"
                                   "search?q=cache:example.com/"));
  // Wrong path.
  EXPECT_EQ(GURL(), GetEmbeddedURL("https://webcache.googleusercontent.com/"
                                   "path?q=cache:example.com/"));
  EXPECT_EQ(GURL(), GetEmbeddedURL("https://webcache.googleusercontent.com/"
                                   "path/search?q=cache:example.com/"));
  // Missing "cache:".
  EXPECT_EQ(GURL(), GetEmbeddedURL("https://webcache.googleusercontent.com/"
                                   "search?q=example.com"));
  // Wrong fingerprint.
  EXPECT_EQ(GURL(), GetEmbeddedURL("https://webcache.googleusercontent.com/"
                                   "search?q=cache:123:example.com/"));
  // Wrong query param.
  EXPECT_EQ(GURL(), GetEmbeddedURL("https://webcache.googleusercontent.com/"
                                   "search?a=cache:example.com/"));
  // Invalid scheme.
  EXPECT_EQ(GURL(), GetEmbeddedURL("https://webcache.googleusercontent.com/"
                                   "search?q=cache:abc://example.com/"));
}

TEST(URLUtilTest, GetEmbeddedURLTranslate) {
  // Base case.
  EXPECT_EQ(GURL("http://example.com"),
            GetEmbeddedURL("https://translate.google.com/path?u=example.com"));
  // Different TLD.
  EXPECT_EQ(GURL("http://example.com"),
            GetEmbeddedURL("https://translate.google.de/path?u=example.com"));
  // Alternate base URL.
  EXPECT_EQ(GURL("http://example.com"),
            GetEmbeddedURL(
                "https://translate.googleusercontent.com/path?u=example.com"));
  // With scheme.
  EXPECT_EQ(
      GURL("http://example.com"),
      GetEmbeddedURL("https://translate.google.com/path?u=http://example.com"));
  // With https scheme.
  EXPECT_EQ(GURL("https://example.com"),
            GetEmbeddedURL(
                "https://translate.google.com/path?u=https://example.com"));
  // With other parameters.
  EXPECT_EQ(
      GURL("http://example.com"),
      GetEmbeddedURL(
          "https://translate.google.com/path?a=asdf&u=example.com&b=fdsa"));

  // Different subdomain is not supported.
  EXPECT_EQ(GURL(), GetEmbeddedURL(
                        "https://translate.foo.google.com/path?u=example.com"));
  EXPECT_EQ(GURL(), GetEmbeddedURL(
                        "https://translate.www.google.com/path?u=example.com"));
  EXPECT_EQ(
      GURL(),
      GetEmbeddedURL("https://translate.google.google.com/path?u=example.com"));
  EXPECT_EQ(GURL(), GetEmbeddedURL(
                        "https://foo.translate.google.com/path?u=example.com"));
  EXPECT_EQ(GURL(),
            GetEmbeddedURL("https://translate2.google.com/path?u=example.com"));
  EXPECT_EQ(GURL(),
            GetEmbeddedURL(
                "https://translate2.googleusercontent.com/path?u=example.com"));
  // Different TLD is not supported for googleusercontent.
  EXPECT_EQ(GURL(),
            GetEmbeddedURL(
                "https://translate.googleusercontent.de/path?u=example.com"));
  // Query parameter ("u=...") is missing.
  EXPECT_EQ(GURL(),
            GetEmbeddedURL("https://translate.google.com/path?t=example.com"));
}

}  // namespace url_util
}  // namespace policy
