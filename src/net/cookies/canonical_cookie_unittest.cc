// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/canonical_cookie.h"

#include "base/optional.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "net/base/features.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_options.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

namespace {

// Helper for testing BuildCookieLine
void MatchCookieLineToVector(
    const std::string& line,
    const std::vector<std::unique_ptr<CanonicalCookie>>& cookies) {
  std::vector<CanonicalCookie> list;
  for (const auto& cookie : cookies)
    list.push_back(*cookie);
  EXPECT_EQ(line, CanonicalCookie::BuildCookieLine(list));
}

}  // namespace

TEST(CanonicalCookieTest, Constructor) {
  GURL url("http://www.example.com/test");
  base::Time current_time = base::Time::Now();

  std::unique_ptr<CanonicalCookie> cookie1(std::make_unique<CanonicalCookie>(
      "A", "2", "www.example.com", "/test", current_time, base::Time(),
      base::Time(), false, false, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT));
  EXPECT_EQ("A", cookie1->Name());
  EXPECT_EQ("2", cookie1->Value());
  EXPECT_EQ("www.example.com", cookie1->Domain());
  EXPECT_EQ("/test", cookie1->Path());
  EXPECT_FALSE(cookie1->IsSecure());
  EXPECT_FALSE(cookie1->IsHttpOnly());
  EXPECT_EQ(CookieSameSite::NO_RESTRICTION, cookie1->SameSite());

  std::unique_ptr<CanonicalCookie> cookie2(std::make_unique<CanonicalCookie>(
      "A", "2", ".www.example.com", "/", current_time, base::Time(),
      base::Time(), false, false, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT));
  EXPECT_EQ("A", cookie2->Name());
  EXPECT_EQ("2", cookie2->Value());
  EXPECT_EQ(".www.example.com", cookie2->Domain());
  EXPECT_EQ("/", cookie2->Path());
  EXPECT_FALSE(cookie2->IsSecure());
  EXPECT_FALSE(cookie2->IsHttpOnly());
  EXPECT_EQ(CookieSameSite::NO_RESTRICTION, cookie2->SameSite());

  auto cookie = std::make_unique<CanonicalCookie>(
      "A", "2", ".www.example.com", "/test", current_time, base::Time(),
      base::Time(), false, false, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT);
  EXPECT_EQ("A", cookie->Name());
  EXPECT_EQ("2", cookie->Value());
  EXPECT_EQ(".www.example.com", cookie->Domain());
  EXPECT_EQ("/test", cookie->Path());
  EXPECT_FALSE(cookie->IsSecure());
  EXPECT_FALSE(cookie->IsHttpOnly());
  EXPECT_EQ(CookieSameSite::NO_RESTRICTION, cookie->SameSite());
}

TEST(CanonicalCookie, CreationCornerCases) {
  base::Time creation_time = base::Time::Now();
  std::unique_ptr<CanonicalCookie> cookie;
  base::Optional<base::Time> server_time = base::nullopt;

  // Space in name.
  cookie = CanonicalCookie::Create(GURL("http://www.example.com/test/foo.html"),
                                   "A C=2", creation_time, server_time);
  EXPECT_TRUE(cookie.get());
  EXPECT_EQ("A C", cookie->Name());

  // Semicolon in path.
  cookie = CanonicalCookie::Create(GURL("http://fool/;/"), "*", creation_time,
                                   server_time);
  EXPECT_TRUE(cookie.get());
}

TEST(CanonicalCookieTest, Create) {
  // Test creating cookies from a cookie string.
  GURL url("http://www.example.com/test/foo.html");
  GURL https_url("https://www.example.com/test/foo.html");
  base::Time creation_time = base::Time::Now();
  base::Optional<base::Time> server_time = base::nullopt;

  std::unique_ptr<CanonicalCookie> cookie(
      CanonicalCookie::Create(url, "A=2", creation_time, server_time));
  EXPECT_EQ("A", cookie->Name());
  EXPECT_EQ("2", cookie->Value());
  EXPECT_EQ("www.example.com", cookie->Domain());
  EXPECT_EQ("/test", cookie->Path());
  EXPECT_FALSE(cookie->IsSecure());

  GURL url2("http://www.foo.com");
  cookie = CanonicalCookie::Create(url2, "B=1", creation_time, server_time);
  EXPECT_EQ("B", cookie->Name());
  EXPECT_EQ("1", cookie->Value());
  EXPECT_EQ("www.foo.com", cookie->Domain());
  EXPECT_EQ("/", cookie->Path());
  EXPECT_FALSE(cookie->IsSecure());

  // Test creating secure cookies. Secure scheme is not checked upon creation,
  // so a URL of any scheme can create a Secure cookie.
  CanonicalCookie::CookieInclusionStatus status;
  cookie = CanonicalCookie::Create(url, "A=2; Secure", creation_time,
                                   server_time, &status);
  EXPECT_TRUE(cookie->IsSecure());

  cookie = CanonicalCookie::Create(https_url, "A=2; Secure", creation_time,
                                   server_time, &status);
  EXPECT_TRUE(cookie->IsSecure());

  // Test creating http only cookies. HttpOnly is not checked upon creation.
  cookie = CanonicalCookie::Create(url, "A=2; HttpOnly", creation_time,
                                   server_time, &status);
  EXPECT_TRUE(cookie->IsHttpOnly());

  cookie =
      CanonicalCookie::Create(url, "A=2; HttpOnly", creation_time, server_time);
  EXPECT_TRUE(cookie->IsHttpOnly());

  // Test creating SameSite cookies. SameSite is not checked upon creation.
  cookie = CanonicalCookie::Create(url, "A=2; SameSite=Strict", creation_time,
                                   server_time);
  ASSERT_TRUE(cookie.get());
  EXPECT_EQ(CookieSameSite::STRICT_MODE, cookie->SameSite());
  cookie = CanonicalCookie::Create(url, "A=2; SameSite=Lax", creation_time,
                                   server_time);
  ASSERT_TRUE(cookie.get());
  EXPECT_EQ(CookieSameSite::LAX_MODE, cookie->SameSite());
  cookie = CanonicalCookie::Create(url, "A=2; SameSite=Extended", creation_time,
                                   server_time);
  ASSERT_TRUE(cookie.get());
  EXPECT_EQ(CookieSameSite::EXTENDED_MODE, cookie->SameSite());
  cookie = CanonicalCookie::Create(url, "A=2; SameSite=None", creation_time,
                                   server_time);
  ASSERT_TRUE(cookie.get());
  EXPECT_EQ(CookieSameSite::NO_RESTRICTION, cookie->SameSite());
  cookie = CanonicalCookie::Create(url, "A=2", creation_time, server_time);
  ASSERT_TRUE(cookie.get());
  EXPECT_EQ(CookieSameSite::UNSPECIFIED, cookie->SameSite());
}

TEST(CanonicalCookieTest, CreateNonStandardSameSite) {
  GURL url("http://www.example.com/test/foo.html");
  base::Time now = base::Time::Now();
  std::unique_ptr<CanonicalCookie> cookie;
  base::Optional<base::Time> server_time = base::nullopt;

  // Non-standard value for the SameSite attribute.
  cookie = CanonicalCookie::Create(url, "A=2; SameSite=NonStandard", now,
                                   server_time);
  EXPECT_TRUE(cookie.get());
  EXPECT_EQ(CookieSameSite::UNSPECIFIED, cookie->SameSite());

  // Omit value for the SameSite attribute.
  cookie = CanonicalCookie::Create(url, "A=2; SameSite", now, server_time);
  EXPECT_TRUE(cookie.get());
  EXPECT_EQ(CookieSameSite::UNSPECIFIED, cookie->SameSite());
}

TEST(CanonicalCookieTest, CreateSameSiteInCrossSiteContexts) {
  GURL url("http://www.example.com/test/foo.html");
  base::Time now = base::Time::Now();
  std::unique_ptr<CanonicalCookie> cookie;
  base::Optional<base::Time> server_time = base::nullopt;

  // A cookie can be created from any SameSiteContext regardless of SameSite
  // value (it is upon setting the cookie that the SameSiteContext comes into
  // effect).
  cookie =
      CanonicalCookie::Create(url, "A=2; SameSite=Strict", now, server_time);
  EXPECT_TRUE(cookie.get());
  cookie = CanonicalCookie::Create(url, "A=2; SameSite=Lax", now, server_time);
  EXPECT_TRUE(cookie.get());
  cookie = CanonicalCookie::Create(url, "A=2; SameSite=None", now, server_time);
  EXPECT_TRUE(cookie.get());
  cookie = CanonicalCookie::Create(url, "A=2;", now, server_time);
  EXPECT_TRUE(cookie.get());
}

TEST(CanonicalCookieTest, CreateHttpOnly) {
  GURL url("http://www.example.com/test/foo.html");
  base::Time now = base::Time::Now();
  base::Optional<base::Time> server_time = base::nullopt;
  CanonicalCookie::CookieInclusionStatus status;

  // An HttpOnly cookie can be created.
  std::unique_ptr<CanonicalCookie> cookie =
      CanonicalCookie::Create(url, "A=2; HttpOnly", now, server_time, &status);
  EXPECT_TRUE(cookie->IsHttpOnly());
  EXPECT_TRUE(status.IsInclude());
}

TEST(CanonicalCookieTest, CreateWithInvalidDomain) {
  GURL url("http://www.example.com/test/foo.html");
  base::Time now = base::Time::Now();
  base::Optional<base::Time> server_time = base::nullopt;
  CanonicalCookie::CookieInclusionStatus status;

  std::unique_ptr<CanonicalCookie> cookie = CanonicalCookie::Create(
      url, "A=2; Domain=wrongdomain.com", now, server_time, &status);
  EXPECT_EQ(nullptr, cookie.get());
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CanonicalCookie::CookieInclusionStatus::EXCLUDE_INVALID_DOMAIN}));
}

TEST(CanonicalCookieTest, EmptyExpiry) {
  GURL url("http://www7.ipdl.inpit.go.jp/Tokujitu/tjkta.ipdl?N0000=108");
  base::Time creation_time = base::Time::Now();
  base::Optional<base::Time> server_time = base::nullopt;

  std::string cookie_line =
      "ACSTM=20130308043820420042; path=/; domain=ipdl.inpit.go.jp; Expires=";
  std::unique_ptr<CanonicalCookie> cookie(
      CanonicalCookie::Create(url, cookie_line, creation_time, server_time));
  EXPECT_TRUE(cookie.get());
  EXPECT_FALSE(cookie->IsPersistent());
  EXPECT_FALSE(cookie->IsExpired(creation_time));
  EXPECT_EQ(base::Time(), cookie->ExpiryDate());

  // With a stale server time
  server_time = creation_time - base::TimeDelta::FromHours(1);
  cookie =
      CanonicalCookie::Create(url, cookie_line, creation_time, server_time);
  EXPECT_TRUE(cookie.get());
  EXPECT_FALSE(cookie->IsPersistent());
  EXPECT_FALSE(cookie->IsExpired(creation_time));
  EXPECT_EQ(base::Time(), cookie->ExpiryDate());

  // With a future server time
  server_time = creation_time + base::TimeDelta::FromHours(1);
  cookie =
      CanonicalCookie::Create(url, cookie_line, creation_time, server_time);
  EXPECT_TRUE(cookie.get());
  EXPECT_FALSE(cookie->IsPersistent());
  EXPECT_FALSE(cookie->IsExpired(creation_time));
  EXPECT_EQ(base::Time(), cookie->ExpiryDate());
}

TEST(CanonicalCookieTest, IsEquivalent) {
  GURL url("https://www.example.com/");
  std::string cookie_name = "A";
  std::string cookie_value = "2EDA-EF";
  std::string cookie_domain = ".www.example.com";
  std::string cookie_path = "/path";
  base::Time creation_time = base::Time::Now();
  base::Time expiration_time = creation_time + base::TimeDelta::FromDays(2);
  bool secure(false);
  bool httponly(false);
  CookieSameSite same_site(CookieSameSite::NO_RESTRICTION);

  // Test that a cookie is equivalent to itself.
  std::unique_ptr<CanonicalCookie> cookie(std::make_unique<CanonicalCookie>(
      cookie_name, cookie_value, cookie_domain, cookie_path, creation_time,
      expiration_time, base::Time(), secure, httponly, same_site,
      COOKIE_PRIORITY_MEDIUM));
  EXPECT_TRUE(cookie->IsEquivalent(*cookie));
  EXPECT_TRUE(cookie->IsEquivalentForSecureCookieMatching(*cookie));

  // Test that two identical cookies are equivalent.
  std::unique_ptr<CanonicalCookie> other_cookie(
      std::make_unique<CanonicalCookie>(
          cookie_name, cookie_value, cookie_domain, cookie_path, creation_time,
          expiration_time, base::Time(), secure, httponly, same_site,
          COOKIE_PRIORITY_MEDIUM));
  EXPECT_TRUE(cookie->IsEquivalent(*other_cookie));
  EXPECT_TRUE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));

  // Tests that use different variations of attribute values that
  // DON'T affect cookie equivalence.
  other_cookie = std::make_unique<CanonicalCookie>(
      cookie_name, "2", cookie_domain, cookie_path, creation_time,
      expiration_time, base::Time(), secure, httponly, same_site,
      COOKIE_PRIORITY_HIGH);
  EXPECT_TRUE(cookie->IsEquivalent(*other_cookie));
  EXPECT_TRUE(cookie->IsEquivalentForSecureCookieMatching(*other_cookie));
  EXPECT_TRUE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));

  base::Time other_creation_time =
      creation_time + base::TimeDelta::FromMinutes(2);
  other_cookie = std::make_unique<CanonicalCookie>(
      cookie_name, "2", cookie_domain, cookie_path, other_creation_time,
      expiration_time, base::Time(), secure, httponly, same_site,
      COOKIE_PRIORITY_MEDIUM);
  EXPECT_TRUE(cookie->IsEquivalent(*other_cookie));
  EXPECT_TRUE(cookie->IsEquivalentForSecureCookieMatching(*other_cookie));
  EXPECT_TRUE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));

  other_cookie = std::make_unique<CanonicalCookie>(
      cookie_name, cookie_name, cookie_domain, cookie_path, creation_time,
      expiration_time, base::Time(), true, httponly, same_site,
      COOKIE_PRIORITY_LOW);
  EXPECT_TRUE(cookie->IsEquivalent(*other_cookie));
  EXPECT_TRUE(cookie->IsEquivalentForSecureCookieMatching(*other_cookie));
  EXPECT_TRUE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));

  other_cookie = std::make_unique<CanonicalCookie>(
      cookie_name, cookie_name, cookie_domain, cookie_path, creation_time,
      expiration_time, base::Time(), secure, true, same_site,
      COOKIE_PRIORITY_LOW);
  EXPECT_TRUE(cookie->IsEquivalent(*other_cookie));
  EXPECT_TRUE(cookie->IsEquivalentForSecureCookieMatching(*other_cookie));
  EXPECT_TRUE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));

  other_cookie = std::make_unique<CanonicalCookie>(
      cookie_name, cookie_name, cookie_domain, cookie_path, creation_time,
      expiration_time, base::Time(), secure, httponly,
      CookieSameSite::STRICT_MODE, COOKIE_PRIORITY_LOW);
  EXPECT_TRUE(cookie->IsEquivalent(*other_cookie));
  EXPECT_TRUE(cookie->IsEquivalentForSecureCookieMatching(*other_cookie));
  EXPECT_TRUE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));

  // Cookies whose names mismatch are not equivalent.
  other_cookie = std::make_unique<CanonicalCookie>(
      "B", cookie_value, cookie_domain, cookie_path, creation_time,
      expiration_time, base::Time(), secure, httponly, same_site,
      COOKIE_PRIORITY_MEDIUM);
  EXPECT_FALSE(cookie->IsEquivalent(*other_cookie));
  EXPECT_FALSE(cookie->IsEquivalentForSecureCookieMatching(*other_cookie));
  EXPECT_FALSE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));

  // A domain cookie at 'www.example.com' is not equivalent to a host cookie
  // at the same domain. These are, however, equivalent according to the laxer
  // rules of 'IsEquivalentForSecureCookieMatching'.
  other_cookie = std::make_unique<CanonicalCookie>(
      cookie_name, cookie_value, "www.example.com", cookie_path, creation_time,
      expiration_time, base::Time(), secure, httponly, same_site,
      COOKIE_PRIORITY_MEDIUM);
  EXPECT_TRUE(cookie->IsDomainCookie());
  EXPECT_FALSE(other_cookie->IsDomainCookie());
  EXPECT_FALSE(cookie->IsEquivalent(*other_cookie));
  EXPECT_TRUE(cookie->IsEquivalentForSecureCookieMatching(*other_cookie));
  EXPECT_TRUE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));

  // Likewise, a cookie on 'example.com' is not equivalent to a cookie on
  // 'www.example.com', but they are equivalent for secure cookie matching.
  other_cookie = std::make_unique<CanonicalCookie>(
      cookie_name, cookie_value, ".example.com", cookie_path, creation_time,
      expiration_time, base::Time(), secure, httponly, same_site,
      COOKIE_PRIORITY_MEDIUM);
  EXPECT_FALSE(cookie->IsEquivalent(*other_cookie));
  EXPECT_TRUE(cookie->IsEquivalentForSecureCookieMatching(*other_cookie));
  EXPECT_TRUE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));

  // Paths are a bit more complicated. 'IsEquivalent' requires an exact path
  // match, while secure cookie matching uses a more relaxed 'IsOnPath' check.
  // That is, |cookie| set on '/path' is not equivalent in either way to
  // |other_cookie| set on '/test' or '/path/subpath'. It is, however,
  // equivalent for secure cookie matching to |other_cookie| set on '/'.
  other_cookie = std::make_unique<CanonicalCookie>(
      cookie_name, cookie_value, cookie_domain, "/test", creation_time,
      expiration_time, base::Time(), secure, httponly, same_site,
      COOKIE_PRIORITY_MEDIUM);
  EXPECT_FALSE(cookie->IsEquivalent(*other_cookie));
  EXPECT_FALSE(cookie->IsEquivalentForSecureCookieMatching(*other_cookie));
  EXPECT_FALSE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));

  other_cookie = std::make_unique<CanonicalCookie>(
      cookie_name, cookie_value, cookie_domain, cookie_path + "/subpath",
      creation_time, expiration_time, base::Time(), secure, httponly, same_site,
      COOKIE_PRIORITY_MEDIUM);
  EXPECT_FALSE(cookie->IsEquivalent(*other_cookie));
  EXPECT_FALSE(cookie->IsEquivalentForSecureCookieMatching(*other_cookie));
  EXPECT_TRUE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));

  other_cookie = std::make_unique<CanonicalCookie>(
      cookie_name, cookie_value, cookie_domain, "/", creation_time,
      expiration_time, base::Time(), secure, httponly, same_site,
      COOKIE_PRIORITY_MEDIUM);
  EXPECT_FALSE(cookie->IsEquivalent(*other_cookie));
  EXPECT_TRUE(cookie->IsEquivalentForSecureCookieMatching(*other_cookie));
  EXPECT_FALSE(other_cookie->IsEquivalentForSecureCookieMatching(*cookie));
}

TEST(CanonicalCookieTest, IsDomainMatch) {
  GURL url("http://www.example.com/test/foo.html");
  base::Time creation_time = base::Time::Now();
  base::Optional<base::Time> server_time = base::nullopt;

  std::unique_ptr<CanonicalCookie> cookie(
      CanonicalCookie::Create(url, "A=2", creation_time, server_time));
  EXPECT_TRUE(cookie->IsHostCookie());
  EXPECT_TRUE(cookie->IsDomainMatch("www.example.com"));
  EXPECT_TRUE(cookie->IsDomainMatch("www.example.com"));
  EXPECT_FALSE(cookie->IsDomainMatch("foo.www.example.com"));
  EXPECT_FALSE(cookie->IsDomainMatch("www0.example.com"));
  EXPECT_FALSE(cookie->IsDomainMatch("example.com"));

  cookie = CanonicalCookie::Create(url, "A=2; Domain=www.example.com",
                                   creation_time, server_time);
  EXPECT_TRUE(cookie->IsDomainCookie());
  EXPECT_TRUE(cookie->IsDomainMatch("www.example.com"));
  EXPECT_TRUE(cookie->IsDomainMatch("www.example.com"));
  EXPECT_TRUE(cookie->IsDomainMatch("foo.www.example.com"));
  EXPECT_FALSE(cookie->IsDomainMatch("www0.example.com"));
  EXPECT_FALSE(cookie->IsDomainMatch("example.com"));

  cookie = CanonicalCookie::Create(url, "A=2; Domain=.www.example.com",
                                   creation_time, server_time);
  EXPECT_TRUE(cookie->IsDomainMatch("www.example.com"));
  EXPECT_TRUE(cookie->IsDomainMatch("www.example.com"));
  EXPECT_TRUE(cookie->IsDomainMatch("foo.www.example.com"));
  EXPECT_FALSE(cookie->IsDomainMatch("www0.example.com"));
  EXPECT_FALSE(cookie->IsDomainMatch("example.com"));
}

TEST(CanonicalCookieTest, IsOnPath) {
  base::Time creation_time = base::Time::Now();
  base::Optional<base::Time> server_time = base::nullopt;

  std::unique_ptr<CanonicalCookie> cookie(CanonicalCookie::Create(
      GURL("http://www.example.com"), "A=2", creation_time, server_time));
  EXPECT_TRUE(cookie->IsOnPath("/"));
  EXPECT_TRUE(cookie->IsOnPath("/test"));
  EXPECT_TRUE(cookie->IsOnPath("/test/bar.html"));

  // Test the empty string edge case.
  EXPECT_FALSE(cookie->IsOnPath(std::string()));

  cookie = CanonicalCookie::Create(GURL("http://www.example.com/test/foo.html"),
                                   "A=2", creation_time, server_time);
  EXPECT_FALSE(cookie->IsOnPath("/"));
  EXPECT_TRUE(cookie->IsOnPath("/test"));
  EXPECT_TRUE(cookie->IsOnPath("/test/bar.html"));
  EXPECT_TRUE(cookie->IsOnPath("/test/sample/bar.html"));
}

TEST(CanonicalCookieTest, GetEffectiveSameSite) {
  struct EffectiveSameSiteTestCase {
    CookieSameSite same_site;
    CookieSameSite effective_same_site;
  };

  // Test GetEffectiveSameSite for recently created cookies
  // Session cookie created less than kLaxAllowUnsafeMaxAge ago.
  base::Time now = base::Time::Now();
  base::Time creation_time = now - (kLaxAllowUnsafeMaxAge / 4);
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(features::kSameSiteByDefaultCookies);

    const EffectiveSameSiteTestCase test_cases[] = {
        {CookieSameSite::NO_RESTRICTION, CookieSameSite::NO_RESTRICTION},
        {CookieSameSite::LAX_MODE, CookieSameSite::LAX_MODE},
        {CookieSameSite::STRICT_MODE, CookieSameSite::STRICT_MODE},
        {CookieSameSite::EXTENDED_MODE, CookieSameSite::LAX_MODE},
        {CookieSameSite::UNSPECIFIED, CookieSameSite::NO_RESTRICTION}};

    for (const auto& test_case : test_cases) {
      CanonicalCookie cookie("A", "2", "example.test", "/", creation_time,
                             base::Time(), base::Time(), true /* secure */,
                             false /* httponly */, test_case.same_site,
                             COOKIE_PRIORITY_DEFAULT);
      EXPECT_EQ(test_case.effective_same_site,
                cookie.GetEffectiveSameSiteForTesting());
    }
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(features::kSameSiteByDefaultCookies);

    const EffectiveSameSiteTestCase test_cases[] = {
        {CookieSameSite::NO_RESTRICTION, CookieSameSite::NO_RESTRICTION},
        {CookieSameSite::LAX_MODE, CookieSameSite::LAX_MODE},
        {CookieSameSite::STRICT_MODE, CookieSameSite::STRICT_MODE},
        {CookieSameSite::EXTENDED_MODE, CookieSameSite::LAX_MODE},
        {CookieSameSite::UNSPECIFIED, CookieSameSite::LAX_MODE_ALLOW_UNSAFE}};

    for (const auto& test_case : test_cases) {
      CanonicalCookie cookie("A", "2", "example.test", "/", creation_time,
                             base::Time(), base::Time(), true /* secure */,
                             false /* httponly */, test_case.same_site,
                             COOKIE_PRIORITY_DEFAULT);
      EXPECT_EQ(test_case.effective_same_site,
                cookie.GetEffectiveSameSiteForTesting());
    }
  }

  // Persistent cookie with max age less than kLaxAllowUnsafeMaxAge.
  base::Time expiry_time = creation_time + (kLaxAllowUnsafeMaxAge / 4);
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(features::kSameSiteByDefaultCookies);

    const EffectiveSameSiteTestCase test_cases[] = {
        {CookieSameSite::NO_RESTRICTION, CookieSameSite::NO_RESTRICTION},
        {CookieSameSite::LAX_MODE, CookieSameSite::LAX_MODE},
        {CookieSameSite::STRICT_MODE, CookieSameSite::STRICT_MODE},
        {CookieSameSite::EXTENDED_MODE, CookieSameSite::LAX_MODE},
        {CookieSameSite::UNSPECIFIED, CookieSameSite::NO_RESTRICTION}};

    for (const auto& test_case : test_cases) {
      CanonicalCookie cookie("A", "2", "example.test", "/", creation_time,
                             expiry_time, base::Time(), true /* secure */,
                             false /* httponly */, test_case.same_site,
                             COOKIE_PRIORITY_DEFAULT);
      EXPECT_EQ(test_case.effective_same_site,
                cookie.GetEffectiveSameSiteForTesting());
    }
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(features::kSameSiteByDefaultCookies);

    const EffectiveSameSiteTestCase test_cases[] = {
        {CookieSameSite::NO_RESTRICTION, CookieSameSite::NO_RESTRICTION},
        {CookieSameSite::LAX_MODE, CookieSameSite::LAX_MODE},
        {CookieSameSite::STRICT_MODE, CookieSameSite::STRICT_MODE},
        {CookieSameSite::EXTENDED_MODE, CookieSameSite::LAX_MODE},
        {CookieSameSite::UNSPECIFIED, CookieSameSite::LAX_MODE_ALLOW_UNSAFE}};

    for (const auto& test_case : test_cases) {
      CanonicalCookie cookie("A", "2", "example.test", "/", creation_time,
                             expiry_time, base::Time(), true /* secure */,
                             false /* httponly */, test_case.same_site,
                             COOKIE_PRIORITY_DEFAULT);
      EXPECT_EQ(test_case.effective_same_site,
                cookie.GetEffectiveSameSiteForTesting());
    }
  }

  // Test GetEffectiveSameSite for not-recently-created cookies:
  // Session cookie created more than kLaxAllowUnsafeMaxAge ago.
  creation_time = now - (kLaxAllowUnsafeMaxAge * 4);
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(features::kSameSiteByDefaultCookies);

    const EffectiveSameSiteTestCase test_cases[] = {
        {CookieSameSite::NO_RESTRICTION, CookieSameSite::NO_RESTRICTION},
        {CookieSameSite::LAX_MODE, CookieSameSite::LAX_MODE},
        {CookieSameSite::STRICT_MODE, CookieSameSite::STRICT_MODE},
        {CookieSameSite::EXTENDED_MODE, CookieSameSite::LAX_MODE},
        {CookieSameSite::UNSPECIFIED, CookieSameSite::NO_RESTRICTION}};

    for (const auto& test_case : test_cases) {
      CanonicalCookie cookie("A", "2", "example.test", "/", creation_time,
                             base::Time(), base::Time(), true /* secure */,
                             false /* httponly */, test_case.same_site,
                             COOKIE_PRIORITY_DEFAULT);
      EXPECT_EQ(test_case.effective_same_site,
                cookie.GetEffectiveSameSiteForTesting());
    }
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(features::kSameSiteByDefaultCookies);

    const EffectiveSameSiteTestCase test_cases[] = {
        {CookieSameSite::NO_RESTRICTION, CookieSameSite::NO_RESTRICTION},
        {CookieSameSite::LAX_MODE, CookieSameSite::LAX_MODE},
        {CookieSameSite::STRICT_MODE, CookieSameSite::STRICT_MODE},
        {CookieSameSite::EXTENDED_MODE, CookieSameSite::LAX_MODE},
        {CookieSameSite::UNSPECIFIED, CookieSameSite::LAX_MODE}};

    for (const auto& test_case : test_cases) {
      CanonicalCookie cookie("A", "2", "example.test", "/", creation_time,
                             base::Time(), base::Time(), true /* secure */,
                             false /* httponly */, test_case.same_site,
                             COOKIE_PRIORITY_DEFAULT);
      EXPECT_EQ(test_case.effective_same_site,
                cookie.GetEffectiveSameSiteForTesting());
    }
  }

  // Test GetEffectiveSameSite for not-recently-created cookies:
  // Persistent cookie with max age more than kLaxAllowUnsafeMaxAge, created
  // more than kLaxAllowUnsafeMaxAge ago.
  expiry_time = creation_time + (kLaxAllowUnsafeMaxAge * 8);
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(features::kSameSiteByDefaultCookies);

    const EffectiveSameSiteTestCase test_cases[] = {
        {CookieSameSite::NO_RESTRICTION, CookieSameSite::NO_RESTRICTION},
        {CookieSameSite::LAX_MODE, CookieSameSite::LAX_MODE},
        {CookieSameSite::STRICT_MODE, CookieSameSite::STRICT_MODE},
        {CookieSameSite::EXTENDED_MODE, CookieSameSite::LAX_MODE},
        {CookieSameSite::UNSPECIFIED, CookieSameSite::NO_RESTRICTION}};

    for (const auto& test_case : test_cases) {
      CanonicalCookie cookie("A", "2", "example.test", "/", creation_time,
                             expiry_time, base::Time(), true /* secure */,
                             false /* httponly */, test_case.same_site,
                             COOKIE_PRIORITY_DEFAULT);
      EXPECT_EQ(test_case.effective_same_site,
                cookie.GetEffectiveSameSiteForTesting());
    }
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(features::kSameSiteByDefaultCookies);

    const EffectiveSameSiteTestCase test_cases[] = {
        {CookieSameSite::NO_RESTRICTION, CookieSameSite::NO_RESTRICTION},
        {CookieSameSite::LAX_MODE, CookieSameSite::LAX_MODE},
        {CookieSameSite::STRICT_MODE, CookieSameSite::STRICT_MODE},
        {CookieSameSite::EXTENDED_MODE, CookieSameSite::LAX_MODE},
        {CookieSameSite::UNSPECIFIED, CookieSameSite::LAX_MODE}};

    for (const auto& test_case : test_cases) {
      CanonicalCookie cookie("A", "2", "example.test", "/", creation_time,
                             expiry_time, base::Time(), true /* secure */,
                             false /* httponly */, test_case.same_site,
                             COOKIE_PRIORITY_DEFAULT);
      EXPECT_EQ(test_case.effective_same_site,
                cookie.GetEffectiveSameSiteForTesting());
    }
  }
}

TEST(CanonicalCookieTest, IncludeForRequestURL) {
  GURL url("http://www.example.com");
  base::Time creation_time = base::Time::Now();
  CookieOptions options;
  base::Optional<base::Time> server_time = base::nullopt;

  std::unique_ptr<CanonicalCookie> cookie(
      CanonicalCookie::Create(url, "A=2", creation_time, server_time));
  EXPECT_TRUE(cookie->IncludeForRequestURL(url, options).IsInclude());
  EXPECT_TRUE(cookie
                  ->IncludeForRequestURL(GURL("http://www.example.com/foo/bar"),
                                         options)
                  .IsInclude());
  EXPECT_TRUE(cookie
                  ->IncludeForRequestURL(
                      GURL("https://www.example.com/foo/bar"), options)
                  .IsInclude());
  EXPECT_TRUE(
      cookie->IncludeForRequestURL(GURL("https://sub.example.com"), options)
          .HasExactlyExclusionReasonsForTesting(
              {CanonicalCookie::CookieInclusionStatus::
                   EXCLUDE_DOMAIN_MISMATCH}));
  EXPECT_TRUE(
      cookie->IncludeForRequestURL(GURL("https://sub.www.example.com"), options)
          .HasExactlyExclusionReasonsForTesting(
              {CanonicalCookie::CookieInclusionStatus::
                   EXCLUDE_DOMAIN_MISMATCH}));

  // Test that cookie with a cookie path that does not match the url path are
  // not included.
  cookie = CanonicalCookie::Create(url, "A=2; Path=/foo/bar", creation_time,
                                   server_time);
  EXPECT_TRUE(
      cookie->IncludeForRequestURL(url, options)
          .HasExactlyExclusionReasonsForTesting(
              {CanonicalCookie::CookieInclusionStatus::EXCLUDE_NOT_ON_PATH}));
  EXPECT_TRUE(
      cookie
          ->IncludeForRequestURL(
              GURL("http://www.example.com/foo/bar/index.html"), options)
          .IsInclude());

  // Test that a secure cookie is not included for a non secure URL.
  GURL secure_url("https://www.example.com");
  cookie = CanonicalCookie::Create(secure_url, "A=2; Secure", creation_time,
                                   server_time);
  EXPECT_TRUE(cookie->IsSecure());
  EXPECT_TRUE(cookie->IncludeForRequestURL(secure_url, options).IsInclude());
  EXPECT_TRUE(
      cookie->IncludeForRequestURL(url, options)
          .HasExactlyExclusionReasonsForTesting(
              {CanonicalCookie::CookieInclusionStatus::EXCLUDE_SECURE_ONLY}));

  // Test that http only cookies are only included if the include httponly flag
  // is set on the cookie options.
  options.set_include_httponly();
  cookie =
      CanonicalCookie::Create(url, "A=2; HttpOnly", creation_time, server_time);
  EXPECT_TRUE(cookie->IsHttpOnly());
  EXPECT_TRUE(cookie->IncludeForRequestURL(url, options).IsInclude());
  options.set_exclude_httponly();
  EXPECT_TRUE(
      cookie->IncludeForRequestURL(url, options)
          .HasExactlyExclusionReasonsForTesting(
              {CanonicalCookie::CookieInclusionStatus::EXCLUDE_HTTP_ONLY}));
}

TEST(CanonicalCookieTest, IncludeForRequestURLSameSite) {
  const base::TimeDelta kLongAge = kLaxAllowUnsafeMaxAge * 4;
  const base::TimeDelta kShortAge = kLaxAllowUnsafeMaxAge / 4;

  struct TestCase {
    std::string cookie_line;
    bool is_samesite_by_default_enabled;
    CookieSameSite expected_samesite;
    CookieSameSite expected_effective_samesite;
    CookieOptions::SameSiteCookieContext request_options_samesite_context;
    CanonicalCookie::CookieInclusionStatus expected_inclusion_status;
    base::TimeDelta creation_time_delta = base::TimeDelta();
  } test_cases[] = {
      // SameSite-by-default is not enabled:
      // Strict cookies:
      {"A=2; SameSite=Strict", false, CookieSameSite::STRICT_MODE,
       CookieSameSite::STRICT_MODE,
       CookieOptions::SameSiteCookieContext::CROSS_SITE,
       CanonicalCookie::CookieInclusionStatus(
           CanonicalCookie::CookieInclusionStatus::EXCLUDE_SAMESITE_STRICT)},
      {"A=2; SameSite=Strict", false, CookieSameSite::STRICT_MODE,
       CookieSameSite::STRICT_MODE,
       CookieOptions::SameSiteCookieContext::SAME_SITE_LAX_METHOD_UNSAFE,
       CanonicalCookie::CookieInclusionStatus(
           CanonicalCookie::CookieInclusionStatus::EXCLUDE_SAMESITE_STRICT)},
      {"A=2; SameSite=Strict", false, CookieSameSite::STRICT_MODE,
       CookieSameSite::STRICT_MODE,
       CookieOptions::SameSiteCookieContext::SAME_SITE_LAX,
       CanonicalCookie::CookieInclusionStatus(
           CanonicalCookie::CookieInclusionStatus::EXCLUDE_SAMESITE_STRICT)},
      {"A=2; SameSite=Strict", false, CookieSameSite::STRICT_MODE,
       CookieSameSite::STRICT_MODE,
       CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT,
       CanonicalCookie::CookieInclusionStatus()},
      // Lax cookies:
      {"A=2; SameSite=Lax", false, CookieSameSite::LAX_MODE,
       CookieSameSite::LAX_MODE,
       CookieOptions::SameSiteCookieContext::CROSS_SITE,
       CanonicalCookie::CookieInclusionStatus(
           CanonicalCookie::CookieInclusionStatus::EXCLUDE_SAMESITE_LAX)},
      {"A=2; SameSite=Lax", false, CookieSameSite::LAX_MODE,
       CookieSameSite::LAX_MODE,
       CookieOptions::SameSiteCookieContext::SAME_SITE_LAX_METHOD_UNSAFE,
       CanonicalCookie::CookieInclusionStatus(
           CanonicalCookie::CookieInclusionStatus::EXCLUDE_SAMESITE_LAX)},
      {"A=2; SameSite=Lax", false, CookieSameSite::LAX_MODE,
       CookieSameSite::LAX_MODE,
       CookieOptions::SameSiteCookieContext::SAME_SITE_LAX,
       CanonicalCookie::CookieInclusionStatus()},
      {"A=2; SameSite=Lax", false, CookieSameSite::LAX_MODE,
       CookieSameSite::LAX_MODE,
       CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT,
       CanonicalCookie::CookieInclusionStatus()},
      // None cookies:
      {"A=2; SameSite=None; Secure", false, CookieSameSite::NO_RESTRICTION,
       CookieSameSite::NO_RESTRICTION,
       CookieOptions::SameSiteCookieContext::CROSS_SITE,
       CanonicalCookie::CookieInclusionStatus()},
      {"A=2; SameSite=None; Secure", false, CookieSameSite::NO_RESTRICTION,
       CookieSameSite::NO_RESTRICTION,
       CookieOptions::SameSiteCookieContext::SAME_SITE_LAX_METHOD_UNSAFE,
       CanonicalCookie::CookieInclusionStatus()},
      {"A=2; SameSite=None; Secure", false, CookieSameSite::NO_RESTRICTION,
       CookieSameSite::NO_RESTRICTION,
       CookieOptions::SameSiteCookieContext::SAME_SITE_LAX,
       CanonicalCookie::CookieInclusionStatus()},
      {"A=2; SameSite=None; Secure", false, CookieSameSite::NO_RESTRICTION,
       CookieSameSite::NO_RESTRICTION,
       CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT,
       CanonicalCookie::CookieInclusionStatus()},
      // Unspecified cookies (without SameSite-by-default):
      {"A=2", false, CookieSameSite::UNSPECIFIED,
       CookieSameSite::NO_RESTRICTION,
       CookieOptions::SameSiteCookieContext::CROSS_SITE,
       CanonicalCookie::CookieInclusionStatus::MakeFromReasonsForTesting(
           std::vector<
               CanonicalCookie::CookieInclusionStatus::ExclusionReason>(),
           CanonicalCookie::CookieInclusionStatus::
               WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT)},
      {"A=2", false, CookieSameSite::UNSPECIFIED,
       CookieSameSite::NO_RESTRICTION,
       CookieOptions::SameSiteCookieContext::SAME_SITE_LAX_METHOD_UNSAFE,
       CanonicalCookie::CookieInclusionStatus::MakeFromReasonsForTesting(
           std::vector<
               CanonicalCookie::CookieInclusionStatus::ExclusionReason>(),
           CanonicalCookie::CookieInclusionStatus::
               WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT)},
      {"A=2", false, CookieSameSite::UNSPECIFIED,
       CookieSameSite::NO_RESTRICTION,
       CookieOptions::SameSiteCookieContext::SAME_SITE_LAX,
       CanonicalCookie::CookieInclusionStatus()},
      {"A=2", false, CookieSameSite::UNSPECIFIED,
       CookieSameSite::NO_RESTRICTION,
       CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT,
       CanonicalCookie::CookieInclusionStatus()},

      // SameSite-by-default is enabled:
      // Strict cookies:
      {"A=2; SameSite=Strict", true, CookieSameSite::STRICT_MODE,
       CookieSameSite::STRICT_MODE,
       CookieOptions::SameSiteCookieContext::CROSS_SITE,
       CanonicalCookie::CookieInclusionStatus(
           CanonicalCookie::CookieInclusionStatus::EXCLUDE_SAMESITE_STRICT)},
      {"A=2; SameSite=Strict", true, CookieSameSite::STRICT_MODE,
       CookieSameSite::STRICT_MODE,
       CookieOptions::SameSiteCookieContext::SAME_SITE_LAX_METHOD_UNSAFE,
       CanonicalCookie::CookieInclusionStatus(
           CanonicalCookie::CookieInclusionStatus::EXCLUDE_SAMESITE_STRICT)},
      {"A=2; SameSite=Strict", true, CookieSameSite::STRICT_MODE,
       CookieSameSite::STRICT_MODE,
       CookieOptions::SameSiteCookieContext::SAME_SITE_LAX,
       CanonicalCookie::CookieInclusionStatus(
           CanonicalCookie::CookieInclusionStatus::EXCLUDE_SAMESITE_STRICT)},
      {"A=2; SameSite=Strict", true, CookieSameSite::STRICT_MODE,
       CookieSameSite::STRICT_MODE,
       CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT,
       CanonicalCookie::CookieInclusionStatus()},
      // Lax cookies:
      {"A=2; SameSite=Lax", true, CookieSameSite::LAX_MODE,
       CookieSameSite::LAX_MODE,
       CookieOptions::SameSiteCookieContext::CROSS_SITE,
       CanonicalCookie::CookieInclusionStatus(
           CanonicalCookie::CookieInclusionStatus::EXCLUDE_SAMESITE_LAX)},
      {"A=2; SameSite=Lax", true, CookieSameSite::LAX_MODE,
       CookieSameSite::LAX_MODE,
       CookieOptions::SameSiteCookieContext::SAME_SITE_LAX_METHOD_UNSAFE,
       CanonicalCookie::CookieInclusionStatus(
           CanonicalCookie::CookieInclusionStatus::EXCLUDE_SAMESITE_LAX)},
      {"A=2; SameSite=Lax", true, CookieSameSite::LAX_MODE,
       CookieSameSite::LAX_MODE,
       CookieOptions::SameSiteCookieContext::SAME_SITE_LAX,
       CanonicalCookie::CookieInclusionStatus()},
      {"A=2; SameSite=Lax", true, CookieSameSite::LAX_MODE,
       CookieSameSite::LAX_MODE,
       CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT,
       CanonicalCookie::CookieInclusionStatus()},
      // None cookies:
      {"A=2; SameSite=None; Secure", true, CookieSameSite::NO_RESTRICTION,
       CookieSameSite::NO_RESTRICTION,
       CookieOptions::SameSiteCookieContext::CROSS_SITE,
       CanonicalCookie::CookieInclusionStatus()},
      {"A=2; SameSite=None; Secure", true, CookieSameSite::NO_RESTRICTION,
       CookieSameSite::NO_RESTRICTION,
       CookieOptions::SameSiteCookieContext::SAME_SITE_LAX_METHOD_UNSAFE,
       CanonicalCookie::CookieInclusionStatus()},
      {"A=2; SameSite=None; Secure", true, CookieSameSite::NO_RESTRICTION,
       CookieSameSite::NO_RESTRICTION,
       CookieOptions::SameSiteCookieContext::SAME_SITE_LAX,
       CanonicalCookie::CookieInclusionStatus()},
      {"A=2; SameSite=None; Secure", true, CookieSameSite::NO_RESTRICTION,
       CookieSameSite::NO_RESTRICTION,
       CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT,
       CanonicalCookie::CookieInclusionStatus()},
      // Unspecified recently-created cookies (with SameSite-by-default):
      {"A=2", true, CookieSameSite::UNSPECIFIED,
       CookieSameSite::LAX_MODE_ALLOW_UNSAFE,
       CookieOptions::SameSiteCookieContext::CROSS_SITE,
       CanonicalCookie::CookieInclusionStatus(
           CanonicalCookie::CookieInclusionStatus::
               EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX,
           CanonicalCookie::CookieInclusionStatus::
               WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT),
       kShortAge},
      {"A=2", true, CookieSameSite::UNSPECIFIED,
       CookieSameSite::LAX_MODE_ALLOW_UNSAFE,
       CookieOptions::SameSiteCookieContext::SAME_SITE_LAX_METHOD_UNSAFE,
       CanonicalCookie::CookieInclusionStatus::MakeFromReasonsForTesting(
           std::vector<
               CanonicalCookie::CookieInclusionStatus::ExclusionReason>(),
           CanonicalCookie::CookieInclusionStatus::
               WARN_SAMESITE_UNSPECIFIED_LAX_ALLOW_UNSAFE),
       kShortAge},
      {"A=2", true, CookieSameSite::UNSPECIFIED,
       CookieSameSite::LAX_MODE_ALLOW_UNSAFE,
       CookieOptions::SameSiteCookieContext::SAME_SITE_LAX,
       CanonicalCookie::CookieInclusionStatus(), kShortAge},
      {"A=2", true, CookieSameSite::UNSPECIFIED,
       CookieSameSite::LAX_MODE_ALLOW_UNSAFE,
       CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT,
       CanonicalCookie::CookieInclusionStatus(), kShortAge},
      // Unspecified not-recently-created cookies (with SameSite-by-default):
      {"A=2", true, CookieSameSite::UNSPECIFIED, CookieSameSite::LAX_MODE,
       CookieOptions::SameSiteCookieContext::CROSS_SITE,
       CanonicalCookie::CookieInclusionStatus(
           CanonicalCookie::CookieInclusionStatus::
               EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX,
           CanonicalCookie::CookieInclusionStatus::
               WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT),
       kLongAge},
      {"A=2", true, CookieSameSite::UNSPECIFIED, CookieSameSite::LAX_MODE,
       CookieOptions::SameSiteCookieContext::SAME_SITE_LAX_METHOD_UNSAFE,
       CanonicalCookie::CookieInclusionStatus(
           CanonicalCookie::CookieInclusionStatus::
               EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX,
           CanonicalCookie::CookieInclusionStatus::
               WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT),
       kLongAge},
      {"A=2", true, CookieSameSite::UNSPECIFIED, CookieSameSite::LAX_MODE,
       CookieOptions::SameSiteCookieContext::SAME_SITE_LAX,
       CanonicalCookie::CookieInclusionStatus(), kLongAge},
      {"A=2", true, CookieSameSite::UNSPECIFIED, CookieSameSite::LAX_MODE,
       CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT,
       CanonicalCookie::CookieInclusionStatus(), kLongAge},
  };

  GURL url("https://example.test");

  int length = sizeof(test_cases) / sizeof(test_cases[0]);
  for (int i = 0; i < length; ++i) {
    TestCase test = test_cases[i];
    base::test::ScopedFeatureList feature_list;
    if (test.is_samesite_by_default_enabled) {
      feature_list.InitAndEnableFeature(features::kSameSiteByDefaultCookies);
    } else {
      feature_list.InitAndDisableFeature(features::kSameSiteByDefaultCookies);
    }

    base::Time creation_time = base::Time::Now() - test.creation_time_delta;
    std::unique_ptr<CanonicalCookie> cookie = CanonicalCookie::Create(
        url, test.cookie_line, creation_time, base::nullopt /* server_time */);
    EXPECT_EQ(test.expected_samesite, cookie->SameSite());
    EXPECT_EQ(test.expected_effective_samesite,
              cookie->GetEffectiveSameSiteForTesting())
        << "Test case " << i << " failed.";

    CookieOptions request_options;
    request_options.set_same_site_cookie_context(
        test.request_options_samesite_context);

    EXPECT_EQ(test.expected_inclusion_status,
              cookie->IncludeForRequestURL(url, request_options))
        << "Test case " << i << " failed.";
  }
}

// Test that non-SameSite, insecure cookies are excluded if both
// SameSiteByDefaultCookies and CookiesWithoutSameSiteMustBeSecure are enabled.
TEST(CanonicalCookieTest, IncludeCookiesWithoutSameSiteMustBeSecure) {
  GURL url("https://www.example.com");
  base::Time creation_time = base::Time::Now();
  base::Optional<base::Time> server_time = base::nullopt;
  CookieOptions options;
  std::unique_ptr<CanonicalCookie> cookie;

  // Create the cookie without the experimental options enabled.
  cookie = CanonicalCookie::Create(url, "A=2; SameSite=None", creation_time,
                                   server_time);
  ASSERT_TRUE(cookie.get());
  EXPECT_FALSE(cookie->IsSecure());
  EXPECT_EQ(CookieSameSite::NO_RESTRICTION, cookie->SameSite());
  EXPECT_EQ(CookieSameSite::NO_RESTRICTION,
            cookie->GetEffectiveSameSiteForTesting());

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {features::kSameSiteByDefaultCookies,
         features::kCookiesWithoutSameSiteMustBeSecure} /* enabled_features */,
        {} /* disabled_features */);

    EXPECT_TRUE(cookie->IncludeForRequestURL(url, options)
                    .HasExactlyExclusionReasonsForTesting(
                        {CanonicalCookie::CookieInclusionStatus::
                             EXCLUDE_SAMESITE_NONE_INSECURE}));
  }
}

TEST(CanonicalCookieTest, MultipleExclusionReasons) {
  GURL url("http://www.not-secure.com/foo");
  base::Time creation_time = base::Time::Now();
  base::Optional<base::Time> server_time = base::nullopt;
  CookieOptions options;
  options.set_exclude_httponly();
  options.set_same_site_cookie_context(
      CookieOptions::SameSiteCookieContext::CROSS_SITE);

  // Test IncludeForRequestURL()
  // Note: This is a cookie that should never exist normally, because Create()
  // would weed it out.
  CanonicalCookie cookie1 = CanonicalCookie(
      "name", "value", "other-domain.com", "/bar", creation_time, base::Time(),
      base::Time(), true /* secure */, true /* httponly */,
      CookieSameSite::STRICT_MODE, COOKIE_PRIORITY_DEFAULT);
  EXPECT_TRUE(
      cookie1.IncludeForRequestURL(url, options)
          .HasExactlyExclusionReasonsForTesting(
              {CanonicalCookie::CookieInclusionStatus::EXCLUDE_HTTP_ONLY,
               CanonicalCookie::CookieInclusionStatus::EXCLUDE_SECURE_ONLY,
               CanonicalCookie::CookieInclusionStatus::EXCLUDE_DOMAIN_MISMATCH,
               CanonicalCookie::CookieInclusionStatus::EXCLUDE_NOT_ON_PATH,
               CanonicalCookie::CookieInclusionStatus::
                   EXCLUDE_SAMESITE_STRICT}));

  // Test Create()
  CanonicalCookie::CookieInclusionStatus create_status;
  auto cookie2 = CanonicalCookie::Create(
      url, "__Secure-notactuallysecure=value;Domain=some-other-domain.com",
      creation_time, server_time, &create_status);
  ASSERT_FALSE(cookie2);
  EXPECT_TRUE(create_status.HasExactlyExclusionReasonsForTesting(
      {CanonicalCookie::CookieInclusionStatus::EXCLUDE_INVALID_PREFIX,
       CanonicalCookie::CookieInclusionStatus::EXCLUDE_INVALID_DOMAIN}));

  // Test IsSetPermittedInContext()
  auto cookie3 = CanonicalCookie::Create(
      url, "name=value;HttpOnly;SameSite=Lax", creation_time, server_time);
  ASSERT_TRUE(cookie3);
  EXPECT_TRUE(
      cookie3->IsSetPermittedInContext(options)
          .HasExactlyExclusionReasonsForTesting(
              {CanonicalCookie::CookieInclusionStatus::EXCLUDE_HTTP_ONLY,
               CanonicalCookie::CookieInclusionStatus::EXCLUDE_SAMESITE_LAX}));
}

TEST(CanonicalCookieTest, PartialCompare) {
  GURL url("http://www.example.com");
  base::Time creation_time = base::Time::Now();
  base::Optional<base::Time> server_time = base::nullopt;
  std::unique_ptr<CanonicalCookie> cookie(
      CanonicalCookie::Create(url, "a=b", creation_time, server_time));
  std::unique_ptr<CanonicalCookie> cookie_different_path(
      CanonicalCookie::Create(url, "a=b; path=/foo", creation_time,
                              server_time));
  std::unique_ptr<CanonicalCookie> cookie_different_value(
      CanonicalCookie::Create(url, "a=c", creation_time, server_time));

  // Cookie is equivalent to itself.
  EXPECT_FALSE(cookie->PartialCompare(*cookie));

  // Changing the path affects the ordering.
  EXPECT_TRUE(cookie->PartialCompare(*cookie_different_path));
  EXPECT_FALSE(cookie_different_path->PartialCompare(*cookie));

  // Changing the value does not affect the ordering.
  EXPECT_FALSE(cookie->PartialCompare(*cookie_different_value));
  EXPECT_FALSE(cookie_different_value->PartialCompare(*cookie));

  // Cookies identical for PartialCompare() are equivalent.
  EXPECT_TRUE(cookie->IsEquivalent(*cookie_different_value));
  EXPECT_TRUE(cookie->IsEquivalent(*cookie));
}

TEST(CanonicalCookieTest, SecureCookiePrefix) {
  GURL https_url("https://www.example.test");
  GURL http_url("http://www.example.test");
  base::Time creation_time = base::Time::Now();
  base::Optional<base::Time> server_time = base::nullopt;
  CanonicalCookie::CookieInclusionStatus status;

  // A __Secure- cookie must be Secure.
  EXPECT_FALSE(CanonicalCookie::Create(https_url, "__Secure-A=B", creation_time,
                                       server_time, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CanonicalCookie::CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));
  EXPECT_FALSE(CanonicalCookie::Create(https_url, "__Secure-A=B; httponly",
                                       creation_time, server_time, &status));
  // (EXCLUDE_HTTP_ONLY would be fine, too)
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CanonicalCookie::CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));

  // A typoed prefix does not have to be Secure.
  EXPECT_TRUE(CanonicalCookie::Create(https_url, "__secure-A=B; Secure",
                                      creation_time, server_time));
  EXPECT_TRUE(CanonicalCookie::Create(https_url, "__secure-A=C;", creation_time,
                                      server_time));
  EXPECT_TRUE(CanonicalCookie::Create(https_url, "__SecureA=B; Secure",
                                      creation_time, server_time));
  EXPECT_TRUE(CanonicalCookie::Create(https_url, "__SecureA=C;", creation_time,
                                      server_time));

  // A __Secure- cookie can't be set on a non-secure origin.
  EXPECT_FALSE(CanonicalCookie::Create(http_url, "__Secure-A=B; Secure",
                                       creation_time, server_time, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CanonicalCookie::CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));
}

TEST(CanonicalCookieTest, HostCookiePrefix) {
  GURL https_url("https://www.example.test");
  GURL http_url("http://www.example.test");
  base::Time creation_time = base::Time::Now();
  base::Optional<base::Time> server_time = base::nullopt;
  std::string domain = https_url.host();
  CanonicalCookie::CookieInclusionStatus status;

  // A __Host- cookie must be Secure.
  EXPECT_FALSE(CanonicalCookie::Create(https_url, "__Host-A=B;", creation_time,
                                       server_time, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CanonicalCookie::CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));
  EXPECT_FALSE(CanonicalCookie::Create(
      https_url, "__Host-A=B; Domain=" + domain + "; Path=/;", creation_time,
      server_time, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CanonicalCookie::CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));
  EXPECT_TRUE(CanonicalCookie::Create(https_url, "__Host-A=B; Path=/; Secure;",
                                      creation_time, server_time));

  // A __Host- cookie must be set from a secure scheme.
  EXPECT_FALSE(CanonicalCookie::Create(
      http_url, "__Host-A=B; Domain=" + domain + "; Path=/; Secure;",
      creation_time, server_time, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CanonicalCookie::CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));
  EXPECT_TRUE(CanonicalCookie::Create(https_url, "__Host-A=B; Path=/; Secure;",
                                      creation_time, server_time));

  // A __Host- cookie can't have a Domain.
  EXPECT_FALSE(CanonicalCookie::Create(
      https_url, "__Host-A=B; Domain=" + domain + "; Path=/; Secure;",
      creation_time, server_time, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CanonicalCookie::CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));
  EXPECT_FALSE(CanonicalCookie::Create(
      https_url, "__Host-A=B; Domain=" + domain + "; Secure;", creation_time,
      server_time, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CanonicalCookie::CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));

  // A __Host- cookie may have a domain if it's an IP address that matches the
  // URL.
  EXPECT_TRUE(
      CanonicalCookie::Create(GURL("https://127.0.0.1"),
                              "__Host-A=B; Domain=127.0.0.1; Path=/; Secure;",
                              creation_time, server_time, &status));
  // A __Host- cookie with an IP address domain does not need the domain
  // attribute specified explicitly (just like a normal domain).
  EXPECT_TRUE(CanonicalCookie::Create(GURL("https://127.0.0.1"),
                                      "__Host-A=B; Domain=; Path=/; Secure;",
                                      creation_time, server_time, &status));

  // A __Host- cookie must have a Path of "/".
  EXPECT_FALSE(CanonicalCookie::Create(https_url,
                                       "__Host-A=B; Path=/foo; Secure;",
                                       creation_time, server_time, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CanonicalCookie::CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));
  EXPECT_FALSE(CanonicalCookie::Create(https_url, "__Host-A=B; Secure;",
                                       creation_time, server_time, &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CanonicalCookie::CookieInclusionStatus::EXCLUDE_INVALID_PREFIX}));
  EXPECT_TRUE(CanonicalCookie::Create(https_url, "__Host-A=B; Secure; Path=/;",
                                      creation_time, server_time));

  // Rules don't apply for a typoed prefix.
  EXPECT_TRUE(CanonicalCookie::Create(
      http_url, "__host-A=B; Domain=" + domain + "; Path=/;", creation_time,
      server_time));
  EXPECT_TRUE(CanonicalCookie::Create(
      https_url, "__HostA=B; Domain=" + domain + "; Secure;", creation_time,
      server_time));
}

TEST(CanonicalCookieTest, CanCreateSecureCookiesFromAnyScheme) {
  GURL http_url("http://www.example.com");
  GURL https_url("https://www.example.com");
  base::Time creation_time = base::Time::Now();
  base::Optional<base::Time> server_time = base::nullopt;

  std::unique_ptr<CanonicalCookie> http_cookie_no_secure(
      CanonicalCookie::Create(http_url, "a=b", creation_time, server_time));
  std::unique_ptr<CanonicalCookie> http_cookie_secure(CanonicalCookie::Create(
      http_url, "a=b; Secure", creation_time, server_time));
  std::unique_ptr<CanonicalCookie> https_cookie_no_secure(
      CanonicalCookie::Create(https_url, "a=b", creation_time, server_time));
  std::unique_ptr<CanonicalCookie> https_cookie_secure(CanonicalCookie::Create(
      https_url, "a=b; Secure", creation_time, server_time));

  EXPECT_TRUE(http_cookie_no_secure.get());
  EXPECT_TRUE(http_cookie_secure.get());
  EXPECT_TRUE(https_cookie_no_secure.get());
  EXPECT_TRUE(https_cookie_secure.get());
}

TEST(CanonicalCookieTest, IsCanonical) {
  // Base correct template.
  EXPECT_TRUE(CanonicalCookie("A", "B", "x.y", "/path", base::Time(),
                              base::Time(), base::Time(), false, false,
                              CookieSameSite::NO_RESTRICTION,
                              COOKIE_PRIORITY_LOW)
                  .IsCanonical());

  // Newline in name.
  EXPECT_FALSE(CanonicalCookie("A\n", "B", "x.y", "/path", base::Time(),
                               base::Time(), base::Time(), false, false,
                               CookieSameSite::NO_RESTRICTION,
                               COOKIE_PRIORITY_LOW)
                   .IsCanonical());

  // Carriage return in name.
  EXPECT_FALSE(CanonicalCookie("A\r", "B", "x.y", "/path", base::Time(),
                               base::Time(), base::Time(), false, false,
                               CookieSameSite::NO_RESTRICTION,
                               COOKIE_PRIORITY_LOW)
                   .IsCanonical());

  // Null character in name.
  EXPECT_FALSE(CanonicalCookie(std::string("A\0Z", 3), "B", "x.y", "/path",
                               base::Time(), base::Time(), base::Time(), false,
                               false, CookieSameSite::NO_RESTRICTION,
                               COOKIE_PRIORITY_LOW)
                   .IsCanonical());

  // Name begins with whitespace.
  EXPECT_FALSE(CanonicalCookie(" A", "B", "x.y", "/path", base::Time(),
                               base::Time(), base::Time(), false, false,
                               CookieSameSite::NO_RESTRICTION,
                               COOKIE_PRIORITY_LOW)
                   .IsCanonical());

  // Name ends with whitespace.
  EXPECT_FALSE(CanonicalCookie("A ", "B", "x.y", "/path", base::Time(),
                               base::Time(), base::Time(), false, false,
                               CookieSameSite::NO_RESTRICTION,
                               COOKIE_PRIORITY_LOW)
                   .IsCanonical());

  // Empty name.  (Note this is against the spec but compatible with other
  // browsers.)
  EXPECT_TRUE(CanonicalCookie("", "B", "x.y", "/path", base::Time(),
                              base::Time(), base::Time(), false, false,
                              CookieSameSite::NO_RESTRICTION,
                              COOKIE_PRIORITY_LOW)
                  .IsCanonical());

  // Space in name
  EXPECT_TRUE(CanonicalCookie("A C", "B", "x.y", "/path", base::Time(),
                              base::Time(), base::Time(), false, false,
                              CookieSameSite::NO_RESTRICTION,
                              COOKIE_PRIORITY_LOW)
                  .IsCanonical());

  // Extra space suffixing name.
  EXPECT_FALSE(CanonicalCookie("A ", "B", "x.y", "/path", base::Time(),
                               base::Time(), base::Time(), false, false,
                               CookieSameSite::NO_RESTRICTION,
                               COOKIE_PRIORITY_LOW)
                   .IsCanonical());

  // '=' character in name.
  EXPECT_FALSE(CanonicalCookie("A=", "B", "x.y", "/path", base::Time(),
                               base::Time(), base::Time(), false, false,
                               CookieSameSite::NO_RESTRICTION,
                               COOKIE_PRIORITY_LOW)
                   .IsCanonical());

  // Separator in name.
  EXPECT_FALSE(CanonicalCookie("A;", "B", "x.y", "/path", base::Time(),
                               base::Time(), base::Time(), false, false,
                               CookieSameSite::NO_RESTRICTION,
                               COOKIE_PRIORITY_LOW)
                   .IsCanonical());

  // '=' character in value.
  EXPECT_TRUE(CanonicalCookie("A", "B=", "x.y", "/path", base::Time(),
                              base::Time(), base::Time(), false, false,
                              CookieSameSite::NO_RESTRICTION,
                              COOKIE_PRIORITY_LOW)
                  .IsCanonical());

  // Separator in value.
  EXPECT_FALSE(CanonicalCookie("A", "B;", "x.y", "/path", base::Time(),
                               base::Time(), base::Time(), false, false,
                               CookieSameSite::NO_RESTRICTION,
                               COOKIE_PRIORITY_LOW)
                   .IsCanonical());

  // Separator in domain.
  EXPECT_FALSE(CanonicalCookie("A", "B", ";x.y", "/path", base::Time(),
                               base::Time(), base::Time(), false, false,
                               CookieSameSite::NO_RESTRICTION,
                               COOKIE_PRIORITY_LOW)
                   .IsCanonical());

  // Garbage in domain.
  EXPECT_FALSE(CanonicalCookie("A", "B", "@:&", "/path", base::Time(),
                               base::Time(), base::Time(), false, false,
                               CookieSameSite::NO_RESTRICTION,
                               COOKIE_PRIORITY_LOW)
                   .IsCanonical());

  // Space in domain.
  EXPECT_FALSE(CanonicalCookie("A", "B", "x.y ", "/path", base::Time(),
                               base::Time(), base::Time(), false, false,
                               CookieSameSite::NO_RESTRICTION,
                               COOKIE_PRIORITY_LOW)
                   .IsCanonical());

  // Empty domain.  (This is against cookie spec, but needed for Chrome's
  // out-of-spec use of cookies for extensions; see http://crbug.com/730633.
  EXPECT_TRUE(CanonicalCookie("A", "B", "", "/path", base::Time(), base::Time(),
                              base::Time(), false, false,
                              CookieSameSite::NO_RESTRICTION,
                              COOKIE_PRIORITY_LOW)
                  .IsCanonical());

  // Path does not start with a "/".
  EXPECT_FALSE(CanonicalCookie("A", "B", "x.y", "path", base::Time(),
                               base::Time(), base::Time(), false, false,
                               CookieSameSite::NO_RESTRICTION,
                               COOKIE_PRIORITY_LOW)
                   .IsCanonical());

  // Empty path.
  EXPECT_FALSE(CanonicalCookie("A", "B", "x.y", "", base::Time(), base::Time(),
                               base::Time(), false, false,
                               CookieSameSite::NO_RESTRICTION,
                               COOKIE_PRIORITY_LOW)
                   .IsCanonical());

  // Simple IPv4 address as domain.
  EXPECT_TRUE(CanonicalCookie("A", "B", "1.2.3.4", "/path", base::Time(),
                              base::Time(), base::Time(), false, false,
                              CookieSameSite::NO_RESTRICTION,
                              COOKIE_PRIORITY_LOW)
                  .IsCanonical());

  // NOn-canonical IPv4 address as domain.
  EXPECT_FALSE(CanonicalCookie("A", "B", "01.2.03.4", "/path", base::Time(),
                               base::Time(), base::Time(), false, false,
                               CookieSameSite::NO_RESTRICTION,
                               COOKIE_PRIORITY_LOW)
                   .IsCanonical());

  // Null IPv6 address as domain.
  EXPECT_TRUE(CanonicalCookie("A", "B", "[::]", "/path", base::Time(),
                              base::Time(), base::Time(), false, false,
                              CookieSameSite::NO_RESTRICTION,
                              COOKIE_PRIORITY_LOW)
                  .IsCanonical());

  // Localhost IPv6 address as domain.
  EXPECT_TRUE(CanonicalCookie("A", "B", "[::1]", "/path", base::Time(),
                              base::Time(), base::Time(), false, false,
                              CookieSameSite::NO_RESTRICTION,
                              COOKIE_PRIORITY_LOW)
                  .IsCanonical());

  // Fully speced IPv6 address as domain.
  EXPECT_FALSE(CanonicalCookie(
                   "A", "B", "[2001:0DB8:AC10:FE01:0000:0000:0000:0000]",
                   "/path", base::Time(), base::Time(), base::Time(), false,
                   false, CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW)
                   .IsCanonical());

  // Zero abbreviated IPv6 address as domain.  Not canonical because of leading
  // zeros & uppercase hex letters.
  EXPECT_FALSE(CanonicalCookie("A", "B", "[2001:0DB8:AC10:FE01::]", "/path",
                               base::Time(), base::Time(), base::Time(), false,
                               false, CookieSameSite::NO_RESTRICTION,
                               COOKIE_PRIORITY_LOW)
                   .IsCanonical());

  // Zero prefixes removed IPv6 address as domain.  Not canoncial because of
  // uppercase hex letters.
  EXPECT_FALSE(CanonicalCookie("A", "B", "[2001:DB8:AC10:FE01::]", "/path",
                               base::Time(), base::Time(), base::Time(), false,
                               false, CookieSameSite::NO_RESTRICTION,
                               COOKIE_PRIORITY_LOW)
                   .IsCanonical());

  // Lowercased hex IPv6 address as domain.
  EXPECT_TRUE(CanonicalCookie("A", "B", "[2001:db8:ac10:fe01::]", "/path",
                              base::Time(), base::Time(), base::Time(), false,
                              false, CookieSameSite::NO_RESTRICTION,
                              COOKIE_PRIORITY_LOW)
                  .IsCanonical());

  // Properly formatted host cookie.
  EXPECT_TRUE(CanonicalCookie("__Host-A", "B", "x.y", "/", base::Time(),
                              base::Time(), base::Time(), true, false,
                              CookieSameSite::NO_RESTRICTION,
                              COOKIE_PRIORITY_LOW)
                  .IsCanonical());

  // Insecure host cookie.
  EXPECT_FALSE(CanonicalCookie("__Host-A", "B", "x.y", "/", base::Time(),
                               base::Time(), base::Time(), false, false,
                               CookieSameSite::NO_RESTRICTION,
                               COOKIE_PRIORITY_LOW)
                   .IsCanonical());

  // Host cookie with non-null path.
  EXPECT_FALSE(CanonicalCookie("__Host-A", "B", "x.y", "/path", base::Time(),
                               base::Time(), base::Time(), true, false,
                               CookieSameSite::NO_RESTRICTION,
                               COOKIE_PRIORITY_LOW)
                   .IsCanonical());

  // Host cookie with empty domain.
  EXPECT_FALSE(CanonicalCookie("__Host-A", "B", "", "/", base::Time(),
                               base::Time(), base::Time(), true, false,
                               CookieSameSite::NO_RESTRICTION,
                               COOKIE_PRIORITY_LOW)
                   .IsCanonical());

  // Host cookie with period prefixed domain.
  EXPECT_FALSE(CanonicalCookie("__Host-A", "B", ".x.y", "/", base::Time(),
                               base::Time(), base::Time(), true, false,
                               CookieSameSite::NO_RESTRICTION,
                               COOKIE_PRIORITY_LOW)
                   .IsCanonical());

  // Properly formatted secure cookie.
  EXPECT_TRUE(CanonicalCookie("__Secure-A", "B", "x.y", "/", base::Time(),
                              base::Time(), base::Time(), true, false,
                              CookieSameSite::NO_RESTRICTION,
                              COOKIE_PRIORITY_LOW)
                  .IsCanonical());

  // Insecure secure cookie.
  EXPECT_FALSE(CanonicalCookie("__Secure-A", "B", "x.y", "/", base::Time(),
                               base::Time(), base::Time(), false, false,
                               CookieSameSite::NO_RESTRICTION,
                               COOKIE_PRIORITY_LOW)
                   .IsCanonical());
}

TEST(CanonicalCookieTest, TestSetCreationDate) {
  CanonicalCookie cookie("A", "B", "x.y", "/path", base::Time(), base::Time(),
                         base::Time(), false, false,
                         CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW);
  EXPECT_TRUE(cookie.CreationDate().is_null());

  base::Time now(base::Time::Now());
  cookie.SetCreationDate(now);
  EXPECT_EQ(now, cookie.CreationDate());
}

TEST(CanonicalCookieTest, TestPrefixHistograms) {
  base::HistogramTester histograms;
  const char kCookiePrefixHistogram[] = "Cookie.CookiePrefix";
  const char kCookiePrefixBlockedHistogram[] = "Cookie.CookiePrefixBlocked";
  GURL https_url("https://www.example.test");
  base::Time creation_time = base::Time::Now();
  base::Optional<base::Time> server_time = base::nullopt;

  EXPECT_FALSE(CanonicalCookie::Create(https_url, "__Host-A=B;", creation_time,
                                       server_time));

  histograms.ExpectBucketCount(kCookiePrefixHistogram,
                               CanonicalCookie::COOKIE_PREFIX_HOST, 1);
  histograms.ExpectBucketCount(kCookiePrefixBlockedHistogram,
                               CanonicalCookie::COOKIE_PREFIX_HOST, 1);

  EXPECT_TRUE(CanonicalCookie::Create(https_url, "__Host-A=B; Path=/; Secure",
                                      creation_time, server_time));
  histograms.ExpectBucketCount(kCookiePrefixHistogram,
                               CanonicalCookie::COOKIE_PREFIX_HOST, 2);
  histograms.ExpectBucketCount(kCookiePrefixBlockedHistogram,
                               CanonicalCookie::COOKIE_PREFIX_HOST, 1);
  EXPECT_TRUE(CanonicalCookie::Create(https_url, "__HostA=B; Path=/; Secure",
                                      creation_time, server_time));
  histograms.ExpectBucketCount(kCookiePrefixHistogram,
                               CanonicalCookie::COOKIE_PREFIX_HOST, 2);
  histograms.ExpectBucketCount(kCookiePrefixBlockedHistogram,
                               CanonicalCookie::COOKIE_PREFIX_HOST, 1);

  EXPECT_FALSE(CanonicalCookie::Create(https_url, "__Secure-A=B;",
                                       creation_time, server_time));

  histograms.ExpectBucketCount(kCookiePrefixHistogram,
                               CanonicalCookie::COOKIE_PREFIX_SECURE, 1);
  histograms.ExpectBucketCount(kCookiePrefixBlockedHistogram,
                               CanonicalCookie::COOKIE_PREFIX_SECURE, 1);
  EXPECT_TRUE(CanonicalCookie::Create(https_url, "__Secure-A=B; Path=/; Secure",
                                      creation_time, server_time));
  histograms.ExpectBucketCount(kCookiePrefixHistogram,
                               CanonicalCookie::COOKIE_PREFIX_SECURE, 2);
  histograms.ExpectBucketCount(kCookiePrefixBlockedHistogram,
                               CanonicalCookie::COOKIE_PREFIX_SECURE, 1);
  EXPECT_TRUE(CanonicalCookie::Create(https_url, "__SecureA=B; Path=/; Secure",
                                      creation_time, server_time));
  histograms.ExpectBucketCount(kCookiePrefixHistogram,
                               CanonicalCookie::COOKIE_PREFIX_SECURE, 2);
  histograms.ExpectBucketCount(kCookiePrefixBlockedHistogram,
                               CanonicalCookie::COOKIE_PREFIX_SECURE, 1);
}

TEST(CanonicalCookieTest, BuildCookieLine) {
  std::vector<std::unique_ptr<CanonicalCookie>> cookies;
  GURL url("https://example.com/");
  base::Time now = base::Time::Now();
  base::Optional<base::Time> server_time = base::nullopt;
  MatchCookieLineToVector("", cookies);

  cookies.push_back(CanonicalCookie::Create(url, "A=B", now, server_time));
  MatchCookieLineToVector("A=B", cookies);
  // Nameless cookies are sent back without a prefixed '='.
  cookies.push_back(CanonicalCookie::Create(url, "C", now, server_time));
  MatchCookieLineToVector("A=B; C", cookies);
  // Cookies separated by ';'.
  cookies.push_back(CanonicalCookie::Create(url, "D=E", now, server_time));
  MatchCookieLineToVector("A=B; C; D=E", cookies);
  // BuildCookieLine doesn't reorder the list, it relies on the caller to do so.
  cookies.push_back(CanonicalCookie::Create(
      url, "F=G", now - base::TimeDelta::FromSeconds(1), server_time));
  MatchCookieLineToVector("A=B; C; D=E; F=G", cookies);
  // BuildCookieLine doesn't deduplicate.
  cookies.push_back(CanonicalCookie::Create(
      url, "D=E", now - base::TimeDelta::FromSeconds(2), server_time));
  MatchCookieLineToVector("A=B; C; D=E; F=G; D=E", cookies);
}

// Confirm that input arguments are reflected in the output cookie.
TEST(CanonicalCookieTest, CreateSanitizedCookie_Inputs) {
  base::Time two_hours_ago = base::Time::Now() - base::TimeDelta::FromHours(2);
  base::Time one_hour_ago = base::Time::Now() - base::TimeDelta::FromHours(1);
  base::Time one_hour_from_now =
      base::Time::Now() + base::TimeDelta::FromHours(1);

  std::unique_ptr<CanonicalCookie> cc;
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", std::string(), "/foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT);
  EXPECT_TRUE(cc);
  EXPECT_EQ("A", cc->Name());
  EXPECT_EQ("B", cc->Value());
  EXPECT_EQ("www.foo.com", cc->Domain());
  EXPECT_EQ("/foo", cc->Path());
  EXPECT_EQ(base::Time(), cc->CreationDate());
  EXPECT_EQ(base::Time(), cc->LastAccessDate());
  EXPECT_EQ(base::Time(), cc->ExpiryDate());
  EXPECT_FALSE(cc->IsSecure());
  EXPECT_FALSE(cc->IsHttpOnly());
  EXPECT_EQ(CookieSameSite::NO_RESTRICTION, cc->SameSite());
  EXPECT_EQ(COOKIE_PRIORITY_MEDIUM, cc->Priority());
  EXPECT_FALSE(cc->IsDomainCookie());

  // Creation date
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", std::string(), "/foo",
      two_hours_ago, base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT);
  EXPECT_TRUE(cc);
  EXPECT_EQ(two_hours_ago, cc->CreationDate());

  // Last access date
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", std::string(), "/foo",
      two_hours_ago, base::Time(), one_hour_ago, false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT);
  EXPECT_TRUE(cc);
  EXPECT_EQ(one_hour_ago, cc->LastAccessDate());

  // Expiry
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", std::string(), "/foo",
      base::Time(), one_hour_from_now, base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT);
  EXPECT_TRUE(cc);
  EXPECT_EQ(one_hour_from_now, cc->ExpiryDate());

  // Secure
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", std::string(), "/foo",
      base::Time(), base::Time(), base::Time(), true /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT);
  EXPECT_TRUE(cc);
  EXPECT_TRUE(cc->IsSecure());

  // Httponly
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", std::string(), "/foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      true /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT);
  EXPECT_TRUE(cc);
  EXPECT_TRUE(cc->IsHttpOnly());

  // Same site
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", std::string(), "/foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::LAX_MODE, COOKIE_PRIORITY_DEFAULT);
  EXPECT_TRUE(cc);
  EXPECT_EQ(CookieSameSite::LAX_MODE, cc->SameSite());

  // Priority
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", std::string(), "/foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_LOW);
  EXPECT_TRUE(cc);
  EXPECT_EQ(COOKIE_PRIORITY_LOW, cc->Priority());

  // Domain cookie
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", "www.foo.com", "/foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT);
  EXPECT_TRUE(cc);
  EXPECT_TRUE(cc->IsDomainCookie());
}

// Make sure sanitization and blocking of cookies works correctly.
TEST(CanonicalCookieTest, CreateSanitizedCookie_Logic) {
  base::Time two_hours_ago = base::Time::Now() - base::TimeDelta::FromHours(2);
  base::Time one_hour_ago = base::Time::Now() - base::TimeDelta::FromHours(1);
  base::Time one_hour_from_now =
      base::Time::Now() + base::TimeDelta::FromHours(1);

  // Simple path and domain variations.
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), "A", "B", std::string(), "/foo",
      one_hour_ago, one_hour_from_now, base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT));
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/bar"), "C", "D", "www.foo.com", "/",
      two_hours_ago, base::Time(), one_hour_ago, false /*secure*/,
      true /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT));
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "E", "F", std::string(), std::string(),
      base::Time(), base::Time(), base::Time(), true /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT));

  // Test the file:// protocol.
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("file:///"), "A", "B", std::string(), "/foo", one_hour_ago,
      one_hour_from_now, base::Time(), false /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT));
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("file:///home/user/foo.txt"), "A", "B", std::string(), "/foo",
      one_hour_ago, one_hour_from_now, base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("file:///home/user/foo.txt"), "A", "B", "home", "/foo", one_hour_ago,
      one_hour_from_now, base::Time(), false /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT));

  // Test that malformed attributes fail to set the cookie.
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), " A", "B", std::string(), "/foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), "A;", "B", std::string(), "/foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), "A=", "B", std::string(), "/foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), "A\x07", "B", std::string(), "/foo",
      one_hour_ago, one_hour_from_now, base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com"), "A", " B", std::string(), "/foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com"), "A", "\x0fZ", std::string(), "/foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com"), "A", "B", "www.foo.com ", "/foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), "A", "B", "foo.ozzzzzzle", "/foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), "A", "B", std::string(), "foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com"), "A", "B", std::string(), "/foo ",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), "A", "B", "%2Efoo.com", "/foo",
      one_hour_ago, one_hour_from_now, base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://domaintest.%E3%81%BF%E3%82%93%E3%81%AA"), "A", "B",
      "domaintest.%E3%81%BF%E3%82%93%E3%81%AA", "/foo", base::Time(),
      base::Time(), base::Time(), false /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT));

  std::unique_ptr<CanonicalCookie> cc;

  // Confirm that setting domain cookies with or without leading periods,
  // or on domains different from the URL's, functions correctly.
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), "A", "B", "www.foo.com", "/foo",
      one_hour_ago, one_hour_from_now, base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT);
  ASSERT_TRUE(cc);
  EXPECT_TRUE(cc->IsDomainCookie());
  EXPECT_EQ(".www.foo.com", cc->Domain());

  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), "A", "B", ".www.foo.com", "/foo",
      one_hour_ago, one_hour_from_now, base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT);
  ASSERT_TRUE(cc);
  EXPECT_TRUE(cc->IsDomainCookie());
  EXPECT_EQ(".www.foo.com", cc->Domain());

  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), "A", "B", ".foo.com", "/foo",
      one_hour_ago, one_hour_from_now, base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT);
  ASSERT_TRUE(cc);
  EXPECT_TRUE(cc->IsDomainCookie());
  EXPECT_EQ(".foo.com", cc->Domain());

  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com/foo"), "A", "B", ".www2.www.foo.com", "/foo",
      one_hour_ago, one_hour_from_now, base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT);
  EXPECT_FALSE(cc);

  // Secure/URL Scheme mismatch.
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com"), "A", "B", std::string(), "/foo ",
      base::Time(), base::Time(), base::Time(), true /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT));

  // Null creation date/non-null last access date conflict.
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com"), "A", "B", std::string(), "/foo", base::Time(),
      base::Time(), base::Time::Now(), false /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT));

  // Domain doesn't match URL
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com"), "A", "B", "www.bar.com", "/", base::Time(),
      base::Time(), base::Time(), false /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT));

  // Path with unusual characters escaped.
  cc = CanonicalCookie::CreateSanitizedCookie(
      GURL("http://www.foo.com"), "A", "B", std::string(), "/foo",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT);
  ASSERT_TRUE(cc);
  EXPECT_EQ("/foo%7F", cc->Path());

  // A __Secure- cookie must be Secure.
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "__Secure-A", "B", ".www.foo.com", "/",
      two_hours_ago, one_hour_from_now, one_hour_ago, true, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "__Secure-A", "B", ".www.foo.com", "/",
      two_hours_ago, one_hour_from_now, one_hour_ago, false, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT));

  // A __Host- cookie must be Secure.
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "__Host-A", "B", std::string(), "/",
      two_hours_ago, one_hour_from_now, one_hour_ago, true, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "__Host-A", "B", std::string(), "/",
      two_hours_ago, one_hour_from_now, one_hour_ago, false, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT));

  // A __Host- cookie must have path "/".
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "__Host-A", "B", std::string(), "/",
      two_hours_ago, one_hour_from_now, one_hour_ago, true, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "__Host-A", "B", std::string(), "/foo",
      two_hours_ago, one_hour_from_now, one_hour_ago, true, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT));

  // A __Host- cookie must not specify a domain.
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "__Host-A", "B", std::string(), "/",
      two_hours_ago, one_hour_from_now, one_hour_ago, true, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "__Host-A", "B", ".www.foo.com", "/",
      two_hours_ago, one_hour_from_now, one_hour_ago, true, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT));
  // Without __Host- prefix, this is a valid host cookie because it does not
  // specify a domain.
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", std::string(), "/", two_hours_ago,
      one_hour_from_now, one_hour_ago, true, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT));
  // Without __Host- prefix, this is a valid domain (not host) cookie.
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://www.foo.com"), "A", "B", ".www.foo.com", "/", two_hours_ago,
      one_hour_from_now, one_hour_ago, true, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT));

  // The __Host- prefix should not prevent otherwise-valid host cookies from
  // being accepted.
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://127.0.0.1"), "A", "B", std::string(), "/", two_hours_ago,
      one_hour_from_now, one_hour_ago, true, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT));
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://127.0.0.1"), "__Host-A", "B", std::string(), "/",
      two_hours_ago, one_hour_from_now, one_hour_ago, true, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT));
  // Host cookies should not specify domain unless it is an IP address that
  // matches the URL.
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://127.0.0.1"), "A", "B", "127.0.0.1", "/", two_hours_ago,
      one_hour_from_now, one_hour_ago, true, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT));
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("https://127.0.0.1"), "__Host-A", "B", "127.0.0.1", "/",
      two_hours_ago, one_hour_from_now, one_hour_ago, true, false,
      CookieSameSite::NO_RESTRICTION, CookiePriority::COOKIE_PRIORITY_DEFAULT));

  // Check that CreateSanitizedCookie can gracefully fail on inputs that would
  // crash cookie_util::GetCookieDomainWithString due to failing
  // DCHECKs. Specifically, GetCookieDomainWithString requires that if the
  // domain is empty or the URL's host matches the domain, then the URL's host
  // must pass DomainIsHostOnly; it must not begin with a period.
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://..."), "A", "B", "...", "/", base::Time(), base::Time(),
      base::Time(), false /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://."), "A", "B", std::string(), "/", base::Time(),
      base::Time(), base::Time(), false /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT));
  EXPECT_FALSE(CanonicalCookie::CreateSanitizedCookie(
      GURL("http://.chromium.org"), "A", "B", ".chromium.org", "/",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT));

  // Check that a file URL with an IPv6 host, and matching IPv6 domain, are
  // valid.
  EXPECT_TRUE(CanonicalCookie::CreateSanitizedCookie(
      GURL("file://[A::]"), std::string(), std::string(), "[A::]", "",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT));

  // On Windows, URLs beginning with two backslashes are considered file
  // URLs. On other platforms, they are invalid.
  auto double_backslash_ipv6_cookie = CanonicalCookie::CreateSanitizedCookie(
      GURL("\\\\[A::]"), std::string(), std::string(), "[A::]", "",
      base::Time(), base::Time(), base::Time(), false /*secure*/,
      false /*httponly*/, CookieSameSite::NO_RESTRICTION,
      COOKIE_PRIORITY_DEFAULT);
#if defined(OS_WIN)
  EXPECT_TRUE(double_backslash_ipv6_cookie);
  EXPECT_TRUE(double_backslash_ipv6_cookie->IsCanonical());
#else
  EXPECT_FALSE(double_backslash_ipv6_cookie);
#endif
}

TEST(CanonicalCookieTest, IsSetPermittedInContext) {
  GURL url("http://www.example.com/test");
  base::Time current_time = base::Time::Now();

  CanonicalCookie cookie_scriptable(
      "A", "2", "www.example.com", "/test", current_time, base::Time(),
      base::Time(), true /*secure*/, false /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT);
  CanonicalCookie cookie_httponly(
      "A", "2", "www.example.com", "/test", current_time, base::Time(),
      base::Time(), true /*secure*/, true /*httponly*/,
      CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT);

  CookieOptions context_script;
  CookieOptions context_network;
  context_network.set_include_httponly();

  EXPECT_TRUE(
      cookie_scriptable.IsSetPermittedInContext(context_network).IsInclude());
  EXPECT_TRUE(
      cookie_scriptable.IsSetPermittedInContext(context_script).IsInclude());

  EXPECT_TRUE(
      cookie_httponly.IsSetPermittedInContext(context_network).IsInclude());
  EXPECT_TRUE(
      cookie_httponly.IsSetPermittedInContext(context_script)
          .HasExactlyExclusionReasonsForTesting(
              {CanonicalCookie::CookieInclusionStatus::EXCLUDE_HTTP_ONLY}));

  CookieOptions context_cross_site;
  CookieOptions context_same_site_lax;
  context_same_site_lax.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::SAME_SITE_LAX);
  CookieOptions context_same_site_strict;
  context_same_site_strict.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT);

  {
    CanonicalCookie cookie_same_site_unrestricted(
        "A", "2", "www.example.com", "/test", current_time, base::Time(),
        base::Time(), true /*secure*/, false /*httponly*/,
        CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT);

    EXPECT_TRUE(cookie_same_site_unrestricted
                    .IsSetPermittedInContext(context_cross_site)
                    .IsInclude());
    EXPECT_TRUE(cookie_same_site_unrestricted
                    .IsSetPermittedInContext(context_same_site_lax)
                    .IsInclude());
    EXPECT_TRUE(cookie_same_site_unrestricted
                    .IsSetPermittedInContext(context_same_site_strict)
                    .IsInclude());
  }

  {
    CanonicalCookie cookie_same_site_lax(
        "A", "2", "www.example.com", "/test", current_time, base::Time(),
        base::Time(), true /*secure*/, false /*httponly*/,
        CookieSameSite::LAX_MODE, COOKIE_PRIORITY_DEFAULT);

    EXPECT_TRUE(cookie_same_site_lax.IsSetPermittedInContext(context_cross_site)
                    .HasExactlyExclusionReasonsForTesting(
                        {CanonicalCookie::CookieInclusionStatus::
                             EXCLUDE_SAMESITE_LAX}));
    EXPECT_TRUE(
        cookie_same_site_lax.IsSetPermittedInContext(context_same_site_lax)
            .IsInclude());
    EXPECT_TRUE(
        cookie_same_site_lax.IsSetPermittedInContext(context_same_site_strict)
            .IsInclude());
  }

  {
    CanonicalCookie cookie_same_site_strict(
        "A", "2", "www.example.com", "/test", current_time, base::Time(),
        base::Time(), true /*secure*/, false /*httponly*/,
        CookieSameSite::STRICT_MODE, COOKIE_PRIORITY_DEFAULT);

    // TODO(morlovich): Do compatibility testing on whether set of strict in lax
    // context really should be accepted.
    EXPECT_TRUE(
        cookie_same_site_strict.IsSetPermittedInContext(context_cross_site)
            .HasExactlyExclusionReasonsForTesting(
                {CanonicalCookie::CookieInclusionStatus::
                     EXCLUDE_SAMESITE_STRICT}));
    EXPECT_TRUE(
        cookie_same_site_strict.IsSetPermittedInContext(context_same_site_lax)
            .IsInclude());
    EXPECT_TRUE(cookie_same_site_strict
                    .IsSetPermittedInContext(context_same_site_strict)
                    .IsInclude());
  }

  // Behavior of UNSPECIFIED depends on an experiment.
  CanonicalCookie cookie_same_site_unspecified(
      "A", "2", "www.example.com", "/test", current_time, base::Time(),
      base::Time(), true /*secure*/, false /*httponly*/,
      CookieSameSite::UNSPECIFIED, COOKIE_PRIORITY_DEFAULT);

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(features::kSameSiteByDefaultCookies);

    EXPECT_TRUE(
        cookie_same_site_unspecified.IsSetPermittedInContext(context_cross_site)
            .IsInclude());
    EXPECT_TRUE(cookie_same_site_unspecified
                    .IsSetPermittedInContext(context_same_site_lax)
                    .IsInclude());
    EXPECT_TRUE(cookie_same_site_unspecified
                    .IsSetPermittedInContext(context_same_site_strict)
                    .IsInclude());
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(features::kSameSiteByDefaultCookies);

    EXPECT_TRUE(
        cookie_same_site_unspecified.IsSetPermittedInContext(context_cross_site)
            .HasExactlyExclusionReasonsForTesting(
                {CanonicalCookie::CookieInclusionStatus::
                     EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX}));
    EXPECT_TRUE(cookie_same_site_unspecified
                    .IsSetPermittedInContext(context_same_site_lax)
                    .IsInclude());
    EXPECT_TRUE(cookie_same_site_unspecified
                    .IsSetPermittedInContext(context_same_site_strict)
                    .IsInclude());
  }
}

TEST(CookieInclusionStatusTest, IncludeStatus) {
  int num_exclusion_reasons = static_cast<int>(
      CanonicalCookie::CookieInclusionStatus::NUM_EXCLUSION_REASONS);
  // Zero-argument constructor
  CanonicalCookie::CookieInclusionStatus status;
  EXPECT_TRUE(status.IsValid());
  EXPECT_TRUE(status.IsInclude());
  for (int i = 0; i < num_exclusion_reasons; ++i) {
    EXPECT_FALSE(status.HasExclusionReason(
        static_cast<CanonicalCookie::CookieInclusionStatus::ExclusionReason>(
            i)));
  }
  EXPECT_FALSE(status.HasExclusionReason(
      CanonicalCookie::CookieInclusionStatus::EXCLUDE_UNKNOWN_ERROR));
}

TEST(CookieInclusionStatusTest, ExcludeStatus) {
  int num_exclusion_reasons = static_cast<int>(
      CanonicalCookie::CookieInclusionStatus::NUM_EXCLUSION_REASONS);
  for (int i = 0; i < num_exclusion_reasons; ++i) {
    auto reason =
        static_cast<CanonicalCookie::CookieInclusionStatus::ExclusionReason>(i);
    CanonicalCookie::CookieInclusionStatus status(reason);
    EXPECT_TRUE(status.IsValid());
    EXPECT_FALSE(status.IsInclude());
    EXPECT_TRUE(status.HasExclusionReason(reason));
    for (int j = 0; j < num_exclusion_reasons; ++j) {
      if (i == j)
        continue;
      EXPECT_FALSE(status.HasExclusionReason(
          static_cast<CanonicalCookie::CookieInclusionStatus::ExclusionReason>(
              j)));
    }
  }
}

TEST(CookieInclusionStatusTest, NotValid) {
  CanonicalCookie::CookieInclusionStatus status;
  int num_exclusion_reasons = static_cast<int>(
      CanonicalCookie::CookieInclusionStatus::NUM_EXCLUSION_REASONS);
  status.set_exclusion_reasons(1 << num_exclusion_reasons);
  EXPECT_FALSE(status.IsInclude());
  EXPECT_FALSE(status.IsValid());

  status.set_exclusion_reasons(~0u);
  EXPECT_FALSE(status.IsInclude());
  EXPECT_FALSE(status.IsValid());
}

TEST(CookieInclusionStatusTest, AddExclusionReason) {
  CanonicalCookie::CookieInclusionStatus status;
  status.AddExclusionReason(
      CanonicalCookie::CookieInclusionStatus::EXCLUDE_UNKNOWN_ERROR);
  EXPECT_TRUE(status.IsValid());
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CanonicalCookie::CookieInclusionStatus::EXCLUDE_UNKNOWN_ERROR}));
}

TEST(CookieInclusionStatusTest, RemoveExclusionReason) {
  CanonicalCookie::CookieInclusionStatus status(
      CanonicalCookie::CookieInclusionStatus::EXCLUDE_UNKNOWN_ERROR);
  EXPECT_TRUE(status.IsValid());
  ASSERT_TRUE(status.HasExclusionReason(
      CanonicalCookie::CookieInclusionStatus::EXCLUDE_UNKNOWN_ERROR));

  status.RemoveExclusionReason(
      CanonicalCookie::CookieInclusionStatus::EXCLUDE_UNKNOWN_ERROR);
  EXPECT_TRUE(status.IsValid());
  EXPECT_FALSE(status.HasExclusionReason(
      CanonicalCookie::CookieInclusionStatus::EXCLUDE_UNKNOWN_ERROR));

  // Removing a nonexistent exclusion reason doesn't do anything.
  ASSERT_FALSE(status.HasExclusionReason(
      CanonicalCookie::CookieInclusionStatus::NUM_EXCLUSION_REASONS));
  status.RemoveExclusionReason(
      CanonicalCookie::CookieInclusionStatus::NUM_EXCLUSION_REASONS);
  EXPECT_TRUE(status.IsValid());
  EXPECT_FALSE(status.HasExclusionReason(
      CanonicalCookie::CookieInclusionStatus::NUM_EXCLUSION_REASONS));
}

TEST(CookieInclusionStatusTest, AddExclusionReasonsAndWarningIfAny) {
  CanonicalCookie::CookieInclusionStatus status1;
  CanonicalCookie::CookieInclusionStatus status2;

  status1.set_exclusion_reasons(0b00011111u);
  status2.set_exclusion_reasons(0b11111000u);
  status2.set_warning(
      CanonicalCookie::CookieInclusionStatus::WARN_SAMESITE_NONE_INSECURE);

  status1.AddExclusionReasonsAndWarningIfAny(status2);

  EXPECT_EQ(0b11111111u, status1.exclusion_reasons());
  EXPECT_EQ(CanonicalCookie::CookieInclusionStatus::WARN_SAMESITE_NONE_INSECURE,
            status1.warning());
}

}  // namespace net
