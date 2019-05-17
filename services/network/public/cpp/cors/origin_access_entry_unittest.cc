// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cors/origin_access_entry.h"

#include "services/network/public/mojom/cors_origin_pattern.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

namespace cors {

namespace {

TEST(OriginAccessEntryTest, PublicSuffixListTest) {
  struct TestCase {
    const std::string host;
    OriginAccessEntry::MatchResult expected;
  } inputs[] = {
      {"google.com", OriginAccessEntry::kMatchesOrigin},
      {"hamster.com", OriginAccessEntry::kDoesNotMatchOrigin},
      {"com", OriginAccessEntry::kMatchesOriginButIsPublicSuffix},
  };

  // Implementation expects url::Origin to set the default port for the
  // specified scheme.
  const url::Origin origin = url::Origin::Create(GURL("http://www.google.com"));
  EXPECT_EQ(80, origin.port());

  for (const auto& test : inputs) {
    SCOPED_TRACE(testing::Message() << "Host: " << test.host);
    OriginAccessEntry entry(
        origin.scheme(), test.host, OriginAccessEntry::kPortAny,
        mojom::CorsOriginAccessMatchMode::kAllowSubdomains,
        mojom::CorsOriginAccessMatchPriority::kDefaultPriority);
    EXPECT_EQ(test.expected, entry.MatchesOrigin(origin));
  }
}

TEST(OriginAccessEntryTest, AllowSubdomainsTest) {
  struct TestCase {
    const std::string protocol;
    const std::string host;
    const std::string origin;
    OriginAccessEntry::MatchResult expected_origin;
    OriginAccessEntry::MatchResult expected_domain;
  } inputs[] = {
      {"http", "example.com", "http://example.com/",
       OriginAccessEntry::kMatchesOrigin, OriginAccessEntry::kMatchesOrigin},
      {"http", "example.com", "http://www.example.com/",
       OriginAccessEntry::kMatchesOrigin, OriginAccessEntry::kMatchesOrigin},
      {"http", "example.com", "http://www.www.example.com/",
       OriginAccessEntry::kMatchesOrigin, OriginAccessEntry::kMatchesOrigin},
      {"http", "www.example.com", "http://example.com/",
       OriginAccessEntry::kDoesNotMatchOrigin,
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"http", "www.example.com", "http://www.example.com/",
       OriginAccessEntry::kMatchesOrigin, OriginAccessEntry::kMatchesOrigin},
      {"http", "www.example.com", "http://www.www.example.com/",
       OriginAccessEntry::kMatchesOrigin, OriginAccessEntry::kMatchesOrigin},
      {"http", "com", "http://example.com/",
       OriginAccessEntry::kMatchesOriginButIsPublicSuffix,
       OriginAccessEntry::kMatchesOriginButIsPublicSuffix},
      {"http", "com", "http://www.example.com/",
       OriginAccessEntry::kMatchesOriginButIsPublicSuffix,
       OriginAccessEntry::kMatchesOriginButIsPublicSuffix},
      {"http", "com", "http://www.www.example.com/",
       OriginAccessEntry::kMatchesOriginButIsPublicSuffix,
       OriginAccessEntry::kMatchesOriginButIsPublicSuffix},
      {"https", "example.com", "http://example.com/",
       OriginAccessEntry::kDoesNotMatchOrigin,
       OriginAccessEntry::kMatchesOrigin},
      {"https", "example.com", "http://www.example.com/",
       OriginAccessEntry::kDoesNotMatchOrigin,
       OriginAccessEntry::kMatchesOrigin},
      {"https", "example.com", "http://www.www.example.com/",
       OriginAccessEntry::kDoesNotMatchOrigin,
       OriginAccessEntry::kMatchesOrigin},
      {"http", "example.com", "http://beispiel.de/",
       OriginAccessEntry::kDoesNotMatchOrigin,
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"http", "example.com", "https://beispiel.de/",
       OriginAccessEntry::kDoesNotMatchOrigin,
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"http", "", "http://example.com/", OriginAccessEntry::kMatchesOrigin,
       OriginAccessEntry::kMatchesOrigin},
      {"http", "", "http://beispiel.de/", OriginAccessEntry::kMatchesOrigin,
       OriginAccessEntry::kMatchesOrigin},
      {"https", "", "http://beispiel.de/",
       OriginAccessEntry::kDoesNotMatchOrigin,
       OriginAccessEntry::kMatchesOrigin},
  };

  for (const auto& test : inputs) {
    SCOPED_TRACE(testing::Message()
                 << "Host: " << test.host << ", Origin: " << test.origin);
    url::Origin origin_to_test = url::Origin::Create(GURL(test.origin));
    OriginAccessEntry entry(
        test.protocol, test.host, OriginAccessEntry::kPortAny,
        mojom::CorsOriginAccessMatchMode::kAllowSubdomains,
        mojom::CorsOriginAccessMatchPriority::kDefaultPriority);
    EXPECT_EQ(test.expected_origin, entry.MatchesOrigin(origin_to_test));
    EXPECT_EQ(test.expected_domain, entry.MatchesDomain(origin_to_test.host()));
  }
}

TEST(OriginAccessEntryTest, AllowRegistrableDomainsTest) {
  struct TestCase {
    const std::string protocol;
    const std::string host;
    const std::string origin;
    OriginAccessEntry::MatchResult expected;
  } inputs[] = {
      {"http", "example.com", "http://example.com/",
       OriginAccessEntry::kMatchesOrigin},
      {"http", "example.com", "http://www.example.com/",
       OriginAccessEntry::kMatchesOrigin},
      {"http", "example.com", "http://www.www.example.com/",
       OriginAccessEntry::kMatchesOrigin},
      {"http", "www.example.com", "http://example.com/",
       OriginAccessEntry::kMatchesOrigin},
      {"http", "www.example.com", "http://www.example.com/",
       OriginAccessEntry::kMatchesOrigin},
      {"http", "www.example.com", "http://www.www.example.com/",
       OriginAccessEntry::kMatchesOrigin},
      {"http", "com", "http://example.com/",
       OriginAccessEntry::kMatchesOriginButIsPublicSuffix},
      {"http", "com", "http://www.example.com/",
       OriginAccessEntry::kMatchesOriginButIsPublicSuffix},
      {"http", "com", "http://www.www.example.com/",
       OriginAccessEntry::kMatchesOriginButIsPublicSuffix},
      {"https", "example.com", "http://example.com/",
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"https", "example.com", "http://www.example.com/",
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"https", "example.com", "http://www.www.example.com/",
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"http", "example.com", "http://beispiel.de/",
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"http", "", "http://example.com/", OriginAccessEntry::kMatchesOrigin},
      {"http", "", "http://beispiel.de/", OriginAccessEntry::kMatchesOrigin},
      {"https", "", "http://beispiel.de/",
       OriginAccessEntry::kDoesNotMatchOrigin},
  };

  for (const auto& test : inputs) {
    url::Origin origin_to_test = url::Origin::Create(GURL(test.origin));
    OriginAccessEntry entry(
        test.protocol, test.host, OriginAccessEntry::kPortAny,
        mojom::CorsOriginAccessMatchMode::kAllowRegistrableDomains,
        mojom::CorsOriginAccessMatchPriority::kDefaultPriority);

    SCOPED_TRACE(testing::Message()
                 << "Host: " << test.host << ", Origin: " << test.origin
                 << ", Domain: " << entry.registrable_domain());
    EXPECT_EQ(test.expected, entry.MatchesOrigin(origin_to_test));
  }
}

TEST(OriginAccessEntryTest, AllowRegistrableDomainsTestWithDottedSuffix) {
  struct TestCase {
    const std::string protocol;
    const std::string host;
    const std::string origin;
    OriginAccessEntry::MatchResult expected;
  } inputs[] = {
      {"http", "example.appspot.com", "http://example.appspot.com/",
       OriginAccessEntry::kMatchesOrigin},
      {"http", "example.appspot.com", "http://www.example.appspot.com/",
       OriginAccessEntry::kMatchesOrigin},
      {"http", "example.appspot.com", "http://www.www.example.appspot.com/",
       OriginAccessEntry::kMatchesOrigin},
      {"http", "www.example.appspot.com", "http://example.appspot.com/",
       OriginAccessEntry::kMatchesOrigin},
      {"http", "www.example.appspot.com", "http://www.example.appspot.com/",
       OriginAccessEntry::kMatchesOrigin},
      {"http", "www.example.appspot.com", "http://www.www.example.appspot.com/",
       OriginAccessEntry::kMatchesOrigin},
      {"http", "appspot.com", "http://example.appspot.com/",
       OriginAccessEntry::kMatchesOriginButIsPublicSuffix},
      {"http", "appspot.com", "http://www.example.appspot.com/",
       OriginAccessEntry::kMatchesOriginButIsPublicSuffix},
      {"http", "appspot.com", "http://www.www.example.appspot.com/",
       OriginAccessEntry::kMatchesOriginButIsPublicSuffix},
      {"https", "example.appspot.com", "http://example.appspot.com/",
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"https", "example.appspot.com", "http://www.example.appspot.com/",
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"https", "example.appspot.com", "http://www.www.example.appspot.com/",
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"http", "example.appspot.com", "http://beispiel.de/",
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"http", "", "http://example.appspot.com/",
       OriginAccessEntry::kMatchesOrigin},
      {"http", "", "http://beispiel.de/", OriginAccessEntry::kMatchesOrigin},
      {"https", "", "http://beispiel.de/",
       OriginAccessEntry::kDoesNotMatchOrigin},
  };

  for (const auto& test : inputs) {
    url::Origin origin_to_test = url::Origin::Create(GURL(test.origin));
    OriginAccessEntry entry(
        test.protocol, test.host, OriginAccessEntry::kPortAny,
        mojom::CorsOriginAccessMatchMode::kAllowRegistrableDomains,
        mojom::CorsOriginAccessMatchPriority::kDefaultPriority);

    SCOPED_TRACE(testing::Message()
                 << "Host: " << test.host << ", Origin: " << test.origin
                 << ", Domain: " << entry.registrable_domain());
    EXPECT_EQ(test.expected, entry.MatchesOrigin(origin_to_test));
  }
}

TEST(OriginAccessEntryTest, DisallowSubdomainsTest) {
  struct TestCase {
    const std::string protocol;
    const std::string host;
    const std::string origin;
    OriginAccessEntry::MatchResult expected;
  } inputs[] = {
      {"http", "example.com", "http://example.com/",
       OriginAccessEntry::kMatchesOrigin},
      {"http", "example.com", "http://www.example.com/",
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"http", "example.com", "http://www.www.example.com/",
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"http", "com", "http://example.com/",
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"http", "com", "http://www.example.com/",
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"http", "com", "http://www.www.example.com/",
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"https", "example.com", "http://example.com/",
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"https", "example.com", "http://www.example.com/",
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"https", "example.com", "http://www.www.example.com/",
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"http", "example.com", "http://beispiel.de/",
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"http", "", "http://example.com/",
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"http", "", "http://beispiel.de/",
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"https", "", "http://beispiel.de/",
       OriginAccessEntry::kDoesNotMatchOrigin},
  };

  for (const auto& test : inputs) {
    SCOPED_TRACE(testing::Message()
                 << "Host: " << test.host << ", Origin: " << test.origin);
    url::Origin origin_to_test = url::Origin::Create(GURL(test.origin));
    OriginAccessEntry entry(
        test.protocol, test.host, OriginAccessEntry::kPortAny,
        mojom::CorsOriginAccessMatchMode::kDisallowSubdomains,
        mojom::CorsOriginAccessMatchPriority::kDefaultPriority);
    EXPECT_EQ(test.expected, entry.MatchesOrigin(origin_to_test));
  }
}

TEST(OriginAccessEntryTest, IPAddressTest) {
  struct TestCase {
    const std::string protocol;
    const std::string host;
    bool is_ip_address;
  } inputs[] = {
      {"http", "1.1.1.1", true},
      {"http", "1.1.1.1.1", false},
      {"http", "example.com", false},
      {"http", "hostname.that.ends.with.a.number1", false},
      {"http", "2001:db8::1", false},
      {"http", "[2001:db8::1]", true},
      {"http", "2001:db8::a", false},
      {"http", "[2001:db8::a]", true},
      {"http", "", false},
  };

  for (const auto& test : inputs) {
    SCOPED_TRACE(testing::Message() << "Host: " << test.host);
    OriginAccessEntry entry(
        test.protocol, test.host, OriginAccessEntry::kPortAny,
        mojom::CorsOriginAccessMatchMode::kDisallowSubdomains,
        mojom::CorsOriginAccessMatchPriority::kDefaultPriority);
    EXPECT_EQ(test.is_ip_address, entry.host_is_ip_address()) << test.host;
  }
}

TEST(OriginAccessEntryTest, IPAddressMatchingTest) {
  struct TestCase {
    const std::string protocol;
    const std::string host;
    const std::string origin;
    OriginAccessEntry::MatchResult expected;
  } inputs[] = {
      {"http", "192.0.0.123", "http://192.0.0.123/",
       OriginAccessEntry::kMatchesOrigin},
      {"http", "0.0.123", "http://192.0.0.123/",
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"http", "0.123", "http://192.0.0.123/",
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"http", "1.123", "http://192.0.0.123/",
       OriginAccessEntry::kDoesNotMatchOrigin},
  };

  for (const auto& test : inputs) {
    SCOPED_TRACE(testing::Message()
                 << "Host: " << test.host << ", Origin: " << test.origin);
    url::Origin origin_to_test = url::Origin::Create(GURL(test.origin));
    OriginAccessEntry entry1(
        test.protocol, test.host, OriginAccessEntry::kPortAny,
        mojom::CorsOriginAccessMatchMode::kAllowSubdomains,
        mojom::CorsOriginAccessMatchPriority::kDefaultPriority);
    EXPECT_EQ(test.expected, entry1.MatchesOrigin(origin_to_test));

    OriginAccessEntry entry2(
        test.protocol, test.host, OriginAccessEntry::kPortAny,
        mojom::CorsOriginAccessMatchMode::kDisallowSubdomains,
        mojom::CorsOriginAccessMatchPriority::kDefaultPriority);
    EXPECT_EQ(test.expected, entry2.MatchesOrigin(origin_to_test));
  }
}

TEST(OriginAccessEntryTest, PortMatchingTest) {
  struct TestCase {
    int32_t allow_port;
    const std::string origin;
    OriginAccessEntry::MatchResult expected;
  } inputs[] = {
      {OriginAccessEntry::kPortAny, "http://example.com/",
       OriginAccessEntry::kMatchesOrigin},
      {OriginAccessEntry::kPortAny, "http://example.com:8080/",
       OriginAccessEntry::kMatchesOrigin},
      {80, "http://example.com/", OriginAccessEntry::kMatchesOrigin},
      {8080, "http://example.com/", OriginAccessEntry::kDoesNotMatchOrigin},
      {80, "http://example.com:8080/", OriginAccessEntry::kDoesNotMatchOrigin},
  };

  for (const auto& test : inputs) {
    SCOPED_TRACE(testing::Message()
                 << "Port: " << test.allow_port << ", Origin: " << test.origin);
    url::Origin origin_to_test = url::Origin::Create(GURL(test.origin));
    OriginAccessEntry entry(
        origin_to_test.scheme(), origin_to_test.host(), test.allow_port,
        mojom::CorsOriginAccessMatchMode::kAllowSubdomains,
        mojom::CorsOriginAccessMatchPriority::kDefaultPriority);
    EXPECT_EQ(test.expected, entry.MatchesOrigin(origin_to_test));
  }
}

TEST(OriginAccessEntryTest, CreateCorsOriginPattern) {
  const std::string kProtocol = "https";
  const std::string kDomain = "google.com";
  const auto kMode = mojom::CorsOriginAccessMatchMode::kAllowSubdomains;
  const auto kPriority = mojom::CorsOriginAccessMatchPriority::kDefaultPriority;

  OriginAccessEntry entry(kProtocol, kDomain, OriginAccessEntry::kPortAny,
                          kMode, kPriority);
  mojom::CorsOriginPatternPtr pattern = entry.CreateCorsOriginPattern();
  DCHECK_EQ(kProtocol, pattern->protocol);
  DCHECK_EQ(kDomain, pattern->domain);
  DCHECK_EQ(kMode, pattern->mode);
  DCHECK_EQ(kPriority, pattern->priority);
}

}  // namespace

}  // namespace cors

}  // namespace network
