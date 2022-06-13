// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cookie_settings.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/content_settings/core/common/content_settings.h"
#include "net/base/features.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/canonical_cookie_test_helpers.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/site_for_cookies.h"
#include "services/network/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace network {
namespace {

using testing::_;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::UnorderedElementsAre;

constexpr char kAllowedRequestsHistogram[] =
    "API.StorageAccess.AllowedRequests2";

constexpr char kDomainURL[] = "http://example.com";
constexpr char kURL[] = "http://foo.com";
constexpr char kOtherURL[] = "http://other.com";
constexpr char kSubDomainURL[] = "http://www.corp.example.com";
constexpr char kDomain[] = "example.com";
constexpr char kDotDomain[] = ".example.com";
constexpr char kSubDomain[] = "www.corp.example.com";
constexpr char kOtherDomain[] = "not-example.com";
constexpr char kDomainWildcardPattern[] = "[*.]example.com";
constexpr char kFPSOwnerURL[] = "https://fps-owner.test";
constexpr char kFPSMemberURL[] = "https://fps-member.test";

std::unique_ptr<net::CanonicalCookie> MakeCanonicalCookie(
    const std::string& name,
    const std::string& domain,
    bool sameparty) {
  return net::CanonicalCookie::CreateUnsafeCookieForTesting(
      name, "1", domain, "/" /* path */, base::Time() /* creation */,
      base::Time() /* expiration */, base::Time() /* last_access */,
      true /* secure */, false /* httponly */, net::CookieSameSite::UNSPECIFIED,
      net::CookiePriority::COOKIE_PRIORITY_DEFAULT, sameparty);
}

class CookieSettingsTest : public testing::Test {
 public:
 public:
  CookieSettingsTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  ContentSettingPatternSource CreateSetting(
      const std::string& primary_pattern,
      const std::string& secondary_pattern,
      ContentSetting setting,
      base::Time expiration = base::Time()) {
    return ContentSettingPatternSource(
        ContentSettingsPattern::FromString(primary_pattern),
        ContentSettingsPattern::FromString(secondary_pattern),
        base::Value(setting), std::string(), false /* incognito */, expiration);
  }

  void FastForwardTime(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(CookieSettingsTest, GetCookieSettingDefault) {
  CookieSettings settings;
  EXPECT_EQ(settings.GetCookieSetting(GURL(kURL), GURL(kURL), nullptr),
            CONTENT_SETTING_ALLOW);
}

TEST_F(CookieSettingsTest, GetCookieSetting) {
  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting(kURL, kURL, CONTENT_SETTING_BLOCK)});
  EXPECT_EQ(settings.GetCookieSetting(GURL(kURL), GURL(kURL), nullptr),
            CONTENT_SETTING_BLOCK);
}

TEST_F(CookieSettingsTest, GetCookieSettingMustMatchBothPatterns) {
  CookieSettings settings;
  // This setting needs kOtherURL as the secondary pattern.
  settings.set_content_settings(
      {CreateSetting(kURL, kOtherURL, CONTENT_SETTING_BLOCK)});
  EXPECT_EQ(settings.GetCookieSetting(GURL(kURL), GURL(kURL), nullptr),
            CONTENT_SETTING_ALLOW);

  EXPECT_EQ(settings.GetCookieSetting(GURL(kURL), GURL(kOtherURL), nullptr),
            CONTENT_SETTING_BLOCK);
}

TEST_F(CookieSettingsTest, GetCookieSettingGetsFirstSetting) {
  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting(kURL, kURL, CONTENT_SETTING_BLOCK),
       CreateSetting(kURL, kURL, CONTENT_SETTING_SESSION_ONLY)});
  EXPECT_EQ(settings.GetCookieSetting(GURL(kURL), GURL(kURL), nullptr),
            CONTENT_SETTING_BLOCK);
}

TEST_F(CookieSettingsTest, GetCookieSettingDontBlockThirdParty) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 0);

  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});
  settings.set_block_third_party_cookies(false);
  EXPECT_EQ(settings.GetCookieSetting(GURL(kURL), GURL(kOtherURL), nullptr),
            CONTENT_SETTING_ALLOW);
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 1);
  histogram_tester.ExpectBucketCount(
      kAllowedRequestsHistogram,
      static_cast<int>(net::cookie_util::StorageAccessResult::ACCESS_ALLOWED),
      1);
}

TEST_F(CookieSettingsTest, GetCookieSettingBlockThirdParty) {
  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});
  settings.set_block_third_party_cookies(true);
  EXPECT_EQ(settings.GetCookieSetting(GURL(kURL), GURL(kOtherURL), nullptr),
            CONTENT_SETTING_BLOCK);
}

TEST_F(CookieSettingsTest, GetCookieSettingDontBlockThirdPartyWithException) {
  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting(kURL, kOtherURL, CONTENT_SETTING_ALLOW)});
  settings.set_block_third_party_cookies(true);
  EXPECT_EQ(settings.GetCookieSetting(GURL(kURL), GURL(kOtherURL), nullptr),
            CONTENT_SETTING_ALLOW);
}

// The Storage Access API should unblock storage access that would otherwise be
// blocked.
TEST_F(CookieSettingsTest, GetCookieSettingSAAUnblocks) {
  GURL top_level_url = GURL(kURL);
  GURL url = GURL(kOtherURL);
  GURL third_url = GURL(kDomainURL);

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 0);

  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});
  settings.set_block_third_party_cookies(true);

  settings.set_storage_access_grants(
      {CreateSetting(url.host(), top_level_url.host(), CONTENT_SETTING_ALLOW)});

  // When requesting our setting for the embedder/top-level combination our
  // grant is for access should be allowed. For any other domain pairs access
  // should still be blocked.
  EXPECT_EQ(settings.GetCookieSetting(url, top_level_url, nullptr),
            CONTENT_SETTING_ALLOW);
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 1);
  histogram_tester.ExpectBucketCount(
      kAllowedRequestsHistogram,
      static_cast<int>(net::cookie_util::StorageAccessResult::
                           ACCESS_ALLOWED_STORAGE_ACCESS_GRANT),
      1);

  // Invalid pair the |top_level_url| granting access to |url| is now
  // being loaded under |url| as the top level url.
  EXPECT_EQ(settings.GetCookieSetting(top_level_url, url, nullptr),
            CONTENT_SETTING_BLOCK);
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 2);
  histogram_tester.ExpectBucketCount(
      kAllowedRequestsHistogram,
      static_cast<int>(net::cookie_util::StorageAccessResult::
                           ACCESS_ALLOWED_STORAGE_ACCESS_GRANT),
      1);
  histogram_tester.ExpectBucketCount(
      kAllowedRequestsHistogram,
      static_cast<int>(net::cookie_util::StorageAccessResult::ACCESS_BLOCKED),
      1);

  // Invalid pairs where a |third_url| is used.
  EXPECT_EQ(settings.GetCookieSetting(url, third_url, nullptr),
            CONTENT_SETTING_BLOCK);
  EXPECT_EQ(settings.GetCookieSetting(third_url, top_level_url, nullptr),
            CONTENT_SETTING_BLOCK);

  // If cookies are globally blocked, SAA grants should be ignored.
  {
    settings.set_content_settings(
        {CreateSetting("*", "*", CONTENT_SETTING_BLOCK)});
    settings.set_block_third_party_cookies(true);
    base::HistogramTester histogram_tester;
    EXPECT_EQ(settings.GetCookieSetting(url, top_level_url, nullptr),
              CONTENT_SETTING_BLOCK);
    histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 1);
    histogram_tester.ExpectBucketCount(
        kAllowedRequestsHistogram,
        static_cast<int>(net::cookie_util::StorageAccessResult::ACCESS_BLOCKED),
        1);
  }
}

// Subdomains of the granted embedding url should not gain access if a valid
// grant exists.
TEST_F(CookieSettingsTest, GetCookieSettingSAAResourceWildcards) {
  GURL top_level_url = GURL(kURL);
  GURL url = GURL(kDomainURL);

  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});
  settings.set_block_third_party_cookies(true);

  settings.set_storage_access_grants(
      {CreateSetting(kDomain, top_level_url.host(), CONTENT_SETTING_ALLOW)});

  EXPECT_EQ(settings.GetCookieSetting(url, top_level_url, nullptr),
            CONTENT_SETTING_ALLOW);

  EXPECT_EQ(
      settings.GetCookieSetting(GURL(kSubDomainURL), top_level_url, nullptr),
      CONTENT_SETTING_BLOCK);
}

// Subdomains of the granted top level url should not grant access if a valid
// grant exists.
TEST_F(CookieSettingsTest, GetCookieSettingSAATopLevelWildcards) {
  GURL top_level_url = GURL(kDomainURL);
  GURL url = GURL(kURL);

  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});
  settings.set_block_third_party_cookies(true);

  settings.set_storage_access_grants(
      {CreateSetting(url.host(), kDomain, CONTENT_SETTING_ALLOW)});

  EXPECT_EQ(settings.GetCookieSetting(url, top_level_url, nullptr),
            CONTENT_SETTING_ALLOW);

  EXPECT_EQ(settings.GetCookieSetting(url, GURL(kSubDomainURL), nullptr),
            CONTENT_SETTING_BLOCK);
}

// Any Storage Access API grant should not override an explicit setting to block
// cookie access.
TEST_F(CookieSettingsTest, GetCookieSettingSAARespectsSettings) {
  GURL top_level_url = GURL(kURL);
  GURL url = GURL(kOtherURL);

  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting("*", "*", CONTENT_SETTING_BLOCK)});

  settings.set_storage_access_grants(
      {CreateSetting(url.host(), top_level_url.host(), CONTENT_SETTING_ALLOW)});

  EXPECT_EQ(settings.GetCookieSetting(url, top_level_url, nullptr),
            CONTENT_SETTING_BLOCK);
}

// Once a grant expires access should no longer be given.
TEST_F(CookieSettingsTest, GetCookieSettingSAAExpiredGrant) {
  GURL top_level_url = GURL(kURL);
  GURL url = GURL(kOtherURL);

  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});
  settings.set_block_third_party_cookies(true);

  base::Time expiration_time = base::Time::Now() + base::Seconds(100);
  settings.set_storage_access_grants(
      {CreateSetting(url.host(), top_level_url.host(), CONTENT_SETTING_ALLOW,
                     expiration_time)});

  // When requesting our setting for the embedder/top-level combination our
  // grant is for access should be allowed. For any other domain pairs access
  // should still be blocked.
  EXPECT_EQ(settings.GetCookieSetting(url, top_level_url, nullptr),
            CONTENT_SETTING_ALLOW);

  // If we fastforward past the expiration of our grant the result should be
  // CONTENT_SETTING_BLOCK now.
  FastForwardTime(base::Seconds(101));
  EXPECT_EQ(settings.GetCookieSetting(url, top_level_url, nullptr),
            CONTENT_SETTING_BLOCK);
}

TEST_F(CookieSettingsTest, CreateDeleteCookieOnExitPredicateNoSettings) {
  CookieSettings settings;
  EXPECT_FALSE(settings.CreateDeleteCookieOnExitPredicate());
}

TEST_F(CookieSettingsTest, CreateDeleteCookieOnExitPredicateNoSessionOnly) {
  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});
  EXPECT_FALSE(settings.CreateDeleteCookieOnExitPredicate());
}

TEST_F(CookieSettingsTest, CreateDeleteCookieOnExitPredicateSessionOnly) {
  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting("*", "*", CONTENT_SETTING_SESSION_ONLY)});
  EXPECT_TRUE(settings.CreateDeleteCookieOnExitPredicate().Run(kURL, false));
}

TEST_F(CookieSettingsTest, CreateDeleteCookieOnExitPredicateAllow) {
  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW),
       CreateSetting("*", "*", CONTENT_SETTING_SESSION_ONLY)});
  EXPECT_FALSE(settings.CreateDeleteCookieOnExitPredicate().Run(kURL, false));
}

TEST_F(CookieSettingsTest, GetCookieSettingSecureOriginCookiesAllowed) {
  CookieSettings settings;
  settings.set_secure_origin_cookies_allowed_schemes({"chrome"});
  settings.set_block_third_party_cookies(true);

  EXPECT_EQ(
      settings.GetCookieSetting(GURL("https://foo.com") /* url */,
                                GURL("chrome://foo") /* first_party_url */,
                                nullptr /* source */),
      CONTENT_SETTING_ALLOW);

  EXPECT_EQ(
      settings.GetCookieSetting(GURL("chrome://foo") /* url */,
                                GURL("https://foo.com") /* first_party_url */,
                                nullptr /* source */),
      CONTENT_SETTING_BLOCK);

  EXPECT_EQ(
      settings.GetCookieSetting(GURL("http://foo.com") /* url */,
                                GURL("chrome://foo") /* first_party_url */,
                                nullptr /* source */),
      CONTENT_SETTING_BLOCK);
}

TEST_F(CookieSettingsTest, GetCookieSettingWithThirdPartyCookiesAllowedScheme) {
  CookieSettings settings;
  settings.set_third_party_cookies_allowed_schemes({"chrome-extension"});
  settings.set_block_third_party_cookies(true);

  EXPECT_EQ(settings.GetCookieSetting(
                GURL("http://foo.com") /* url */,
                GURL("chrome-extension://foo") /* first_party_url */,
                nullptr /* source */),
            CONTENT_SETTING_ALLOW);

  EXPECT_EQ(settings.GetCookieSetting(
                GURL("http://foo.com") /* url */,
                GURL("other-scheme://foo") /* first_party_url */,
                nullptr /* source */),
            CONTENT_SETTING_BLOCK);

  EXPECT_EQ(
      settings.GetCookieSetting(GURL("chrome-extension://foo") /* url */,
                                GURL("http://foo.com") /* first_party_url */,
                                nullptr /* source */),
      CONTENT_SETTING_BLOCK);
}

TEST_F(CookieSettingsTest, GetCookieSettingMatchingSchemeCookiesAllowed) {
  CookieSettings settings;
  settings.set_matching_scheme_cookies_allowed_schemes({"chrome-extension"});
  settings.set_block_third_party_cookies(true);

  EXPECT_EQ(settings.GetCookieSetting(
                GURL("chrome-extension://bar") /* url */,
                GURL("chrome-extension://foo") /* first_party_url */,
                nullptr /* source */),
            CONTENT_SETTING_ALLOW);

  EXPECT_EQ(settings.GetCookieSetting(
                GURL("http://foo.com") /* url */,
                GURL("chrome-extension://foo") /* first_party_url */,
                nullptr /* source */),
            CONTENT_SETTING_BLOCK);

  EXPECT_EQ(
      settings.GetCookieSetting(GURL("chrome-extension://foo") /* url */,
                                GURL("http://foo.com") /* first_party_url */,
                                nullptr /* source */),
      CONTENT_SETTING_BLOCK);
}

TEST_F(CookieSettingsTest, LegacyCookieAccessDefault) {
  CookieSettings settings;
  ContentSetting setting;

  settings.GetSettingForLegacyCookieAccess(kDomain, &setting);
  EXPECT_EQ(setting, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(net::CookieAccessSemantics::NONLEGACY,
            settings.GetCookieAccessSemanticsForDomain(kDomain));
}

TEST_F(CookieSettingsTest, CookieAccessSemanticsForDomain) {
  CookieSettings settings;
  settings.set_content_settings_for_legacy_cookie_access(
      {CreateSetting(kDomain, "*", CONTENT_SETTING_ALLOW)});
  const struct {
    net::CookieAccessSemantics status;
    std::string cookie_domain;
  } kTestCases[] = {
      // These two test cases are LEGACY because they match the setting.
      {net::CookieAccessSemantics::LEGACY, kDomain},
      {net::CookieAccessSemantics::LEGACY, kDotDomain},
      // These two test cases default into NONLEGACY.
      // Subdomain does not match pattern.
      {net::CookieAccessSemantics::NONLEGACY, kSubDomain},
      {net::CookieAccessSemantics::NONLEGACY, kOtherDomain}};
  for (const auto& test : kTestCases) {
    EXPECT_EQ(test.status,
              settings.GetCookieAccessSemanticsForDomain(test.cookie_domain));
  }
}

TEST_F(CookieSettingsTest, CookieAccessSemanticsForDomainWithWildcard) {
  CookieSettings settings;
  settings.set_content_settings_for_legacy_cookie_access(
      {CreateSetting(kDomainWildcardPattern, "*", CONTENT_SETTING_ALLOW)});
  const struct {
    net::CookieAccessSemantics status;
    std::string cookie_domain;
  } kTestCases[] = {
      // These three test cases are LEGACY because they match the setting.
      {net::CookieAccessSemantics::LEGACY, kDomain},
      {net::CookieAccessSemantics::LEGACY, kDotDomain},
      // Subdomain also matches pattern.
      {net::CookieAccessSemantics::LEGACY, kSubDomain},
      // This test case defaults into NONLEGACY.
      {net::CookieAccessSemantics::NONLEGACY, kOtherDomain}};
  for (const auto& test : kTestCases) {
    EXPECT_EQ(test.status,
              settings.GetCookieAccessSemanticsForDomain(test.cookie_domain));
  }
}

TEST_F(CookieSettingsTest, IsPrivacyModeEnabled) {
  base::HistogramTester histogram_tester;
  CookieSettings settings;
  settings.set_block_third_party_cookies(true);

  // Enabled for third-party requests.
  EXPECT_TRUE(settings.IsPrivacyModeEnabled(
      GURL(kURL), net::SiteForCookies(), url::Origin::Create(GURL(kOtherURL)),
      net::SamePartyContext::Type::kCrossParty));

  // Enabled for requests with a null site_for_cookies, even if the
  // top_frame_origin matches.
  EXPECT_TRUE(settings.IsPrivacyModeEnabled(
      GURL(kURL), net::SiteForCookies(), url::Origin::Create(GURL(kURL)),
      net::SamePartyContext::Type::kCrossParty));

  // Disabled for first-party requests, if no other rule applies.
  EXPECT_FALSE(settings.IsPrivacyModeEnabled(
      GURL(kURL), net::SiteForCookies::FromUrl(GURL(kURL)),
      url::Origin::Create(GURL(kURL)),
      net::SamePartyContext::Type::kSameParty));

  // Enabled if there's a site-specific rule that blocks access, regardless of
  // the kind of request.
  settings.set_content_settings(
      {CreateSetting(kURL, "*", CONTENT_SETTING_BLOCK)});
  // Third-party requests:
  EXPECT_TRUE(settings.IsPrivacyModeEnabled(
      GURL(kURL), net::SiteForCookies(), url::Origin::Create(GURL(kOtherURL)),
      net::SamePartyContext::Type::kCrossParty));
  // Requests with a null site_for_cookies, but matching top_frame_origin.
  EXPECT_TRUE(settings.IsPrivacyModeEnabled(
      GURL(kURL), net::SiteForCookies(), url::Origin::Create(GURL(kURL)),
      net::SamePartyContext::Type::kCrossParty));
  // First-party requests.
  EXPECT_TRUE(settings.IsPrivacyModeEnabled(
      GURL(kURL), net::SiteForCookies::FromUrl(GURL(kURL)),
      url::Origin::Create(GURL(kURL)),
      net::SamePartyContext::Type::kSameParty));

  // No histogram samples should have been recorded.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Cookie.SameParty.BlockedByThirdPartyCookieBlockingSetting"),
              IsEmpty());
}

TEST_F(CookieSettingsTest, IsPrivacyModeEnabled_SamePartyConsideredFirstParty) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      net::features::kSamePartyCookiesConsideredFirstParty);
  CookieSettings settings;
  settings.set_block_third_party_cookies(true);

  // Enabled for cross-party requests.
  EXPECT_TRUE(
      settings.IsPrivacyModeEnabled(GURL(kFPSMemberURL), net::SiteForCookies(),
                                    url::Origin::Create(GURL(kFPSOwnerURL)),
                                    net::SamePartyContext::Type::kCrossParty));

  // Disabled for cross-site, same-party requests.
  EXPECT_FALSE(
      settings.IsPrivacyModeEnabled(GURL(kFPSMemberURL), net::SiteForCookies(),
                                    url::Origin::Create(GURL(kFPSOwnerURL)),
                                    net::SamePartyContext::Type::kSameParty));

  // Enabled for same-party requests if blocked by a site-specific rule.
  settings.set_content_settings(
      {CreateSetting(kFPSMemberURL, "*", CONTENT_SETTING_BLOCK)});
  EXPECT_TRUE(
      settings.IsPrivacyModeEnabled(GURL(kFPSMemberURL), net::SiteForCookies(),
                                    url::Origin::Create(GURL(kFPSOwnerURL)),
                                    net::SamePartyContext::Type::kSameParty));

  // No histogram samples should have been recorded.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Cookie.SameParty.BlockedByThirdPartyCookieBlockingSetting"),
              IsEmpty());
}

TEST_F(CookieSettingsTest, IsCookieAccessible) {
  base::HistogramTester histogram_tester;
  CookieSettings settings;
  settings.set_block_third_party_cookies(true);

  // Third-party cookies are blocked, the cookie should not be accessible.
  std::unique_ptr<net::CanonicalCookie> non_sameparty_cookie =
      MakeCanonicalCookie("name", kFPSMemberURL, false /* sameparty */);

  EXPECT_FALSE(settings.IsCookieAccessible(
      *non_sameparty_cookie, GURL(kFPSMemberURL), net::SiteForCookies(),
      url::Origin::Create(GURL(kFPSOwnerURL))));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Cookie.SameParty.BlockedByThirdPartyCookieBlockingSetting"),
              IsEmpty());

  // SameParty cookies are not considered first-party, so they should be
  // inaccessible in cross-site contexts.
  std::unique_ptr<net::CanonicalCookie> sameparty_cookie =
      MakeCanonicalCookie("name", kFPSMemberURL, true /* sameparty */);

  EXPECT_FALSE(settings.IsCookieAccessible(
      *sameparty_cookie, GURL(kFPSMemberURL), net::SiteForCookies(),
      url::Origin::Create(GURL(kFPSOwnerURL))));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Cookie.SameParty.BlockedByThirdPartyCookieBlockingSetting"),
              ElementsAre(base::Bucket(/*min=*/1, /*count=*/1)));

  // If the SameParty cookie is blocked by a site-specific setting, it should
  // still be inaccessible.
  settings.set_content_settings(
      {CreateSetting(kFPSMemberURL, "*", CONTENT_SETTING_BLOCK)});
  EXPECT_FALSE(settings.IsCookieAccessible(
      *sameparty_cookie, GURL(kFPSMemberURL), net::SiteForCookies(),
      url::Origin::Create(GURL(kFPSOwnerURL))));
  // It wasn't the third-party cookie blocking setting this time, so we don't
  // record the metric.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Cookie.SameParty.BlockedByThirdPartyCookieBlockingSetting"),
              ElementsAre(base::Bucket(/*min=*/1, /*count=*/1)));
}

TEST_F(CookieSettingsTest, IsCookieAccessible_SamePartyConsideredFirstParty) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      net::features::kSamePartyCookiesConsideredFirstParty);
  CookieSettings settings;
  settings.set_block_third_party_cookies(true);

  // Third-party cookies are blocked, the cookie should not be accessible.
  std::unique_ptr<net::CanonicalCookie> non_sameparty_cookie =
      MakeCanonicalCookie("name", kFPSMemberURL, false /* sameparty */);

  EXPECT_FALSE(settings.IsCookieAccessible(
      *non_sameparty_cookie, GURL(kFPSMemberURL), net::SiteForCookies(),
      url::Origin::Create(GURL(kFPSOwnerURL))));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Cookie.SameParty.BlockedByThirdPartyCookieBlockingSetting"),
              IsEmpty());

  // SameParty cookies are considered first-party, so they should be accessible,
  // even in cross-site contexts.
  std::unique_ptr<net::CanonicalCookie> sameparty_cookie =
      MakeCanonicalCookie("name", kFPSMemberURL, true /* sameparty */);

  EXPECT_TRUE(settings.IsCookieAccessible(
      *sameparty_cookie, GURL(kFPSMemberURL), net::SiteForCookies(),
      url::Origin::Create(GURL(kFPSOwnerURL))));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Cookie.SameParty.BlockedByThirdPartyCookieBlockingSetting"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/1)));

  // If the SameParty cookie is blocked by a site-specific setting, it should
  // not be accessible.
  settings.set_content_settings(
      {CreateSetting(kFPSMemberURL, "*", CONTENT_SETTING_BLOCK)});
  EXPECT_FALSE(settings.IsCookieAccessible(
      *sameparty_cookie, GURL(kFPSMemberURL), net::SiteForCookies(),
      url::Origin::Create(GURL(kFPSOwnerURL))));
  // It wasn't blocked by third-party cookie blocking settings, so we shouldn't
  // record the metric.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Cookie.SameParty.BlockedByThirdPartyCookieBlockingSetting"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/1)));

  // If the SameParty cookie is blocked by the global default setting (i.e. if
  // the user has blocked all cookies), it should not be accessible.
  settings.set_content_settings(
      {CreateSetting("*", "*", CONTENT_SETTING_BLOCK)});
  EXPECT_FALSE(settings.IsCookieAccessible(
      *sameparty_cookie, GURL(kFPSMemberURL), net::SiteForCookies(),
      url::Origin::Create(GURL(kFPSOwnerURL))));
  // It wasn't blocked by third-party cookie blocking settings, so we shouldn't
  // record the metric.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Cookie.SameParty.BlockedByThirdPartyCookieBlockingSetting"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/1)));
}

TEST_F(CookieSettingsTest, AnnotateAndMoveUserBlockedCookies) {
  base::HistogramTester histogram_tester;
  CookieSettings settings;
  settings.set_block_third_party_cookies(true);

  net::CookieAccessResultList maybe_included_cookies = {
      {*MakeCanonicalCookie("third_party", kOtherURL, false /* sameparty */),
       {}},
      {*MakeCanonicalCookie("first_party", kURL, true /* sameparty */), {}}};
  net::CookieAccessResultList excluded_cookies = {
      {*MakeCanonicalCookie("excluded_other", kURL, false /* sameparty */),
       // The ExclusionReason below is irrelevant, as long as there is
       // one.
       net::CookieAccessResult(net::CookieInclusionStatus(
           net::CookieInclusionStatus::ExclusionReason::EXCLUDE_SECURE_ONLY))}};
  url::Origin origin = url::Origin::Create(GURL(kURL));

  EXPECT_FALSE(settings.AnnotateAndMoveUserBlockedCookies(
      GURL(kURL), net::SiteForCookies(), &origin, maybe_included_cookies,
      excluded_cookies));

  EXPECT_THAT(maybe_included_cookies, IsEmpty());
  EXPECT_THAT(
      excluded_cookies,
      UnorderedElementsAre(
          MatchesCookieWithAccessResult(
              net::MatchesCookieWithName("first_party"),
              MatchesCookieAccessResult(
                  HasExactlyExclusionReasonsForTesting(
                      std::vector<net::CookieInclusionStatus::ExclusionReason>{
                          net::CookieInclusionStatus::ExclusionReason::
                              EXCLUDE_USER_PREFERENCES}),
                  _, _, _)),
          MatchesCookieWithAccessResult(
              net::MatchesCookieWithName("excluded_other"),
              MatchesCookieAccessResult(
                  HasExactlyExclusionReasonsForTesting(
                      std::vector<net::CookieInclusionStatus::ExclusionReason>{
                          net::CookieInclusionStatus::ExclusionReason::
                              EXCLUDE_SECURE_ONLY,
                          net::CookieInclusionStatus::ExclusionReason::
                              EXCLUDE_USER_PREFERENCES}),
                  _, _, _)),
          MatchesCookieWithAccessResult(
              net::MatchesCookieWithName("third_party"),
              MatchesCookieAccessResult(
                  HasExactlyExclusionReasonsForTesting(
                      std::vector<net::CookieInclusionStatus::ExclusionReason>{
                          net::CookieInclusionStatus::ExclusionReason::
                              EXCLUDE_USER_PREFERENCES}),
                  _, _, _))));

  // One SameParty cookie was blocked due to 3P cookie blocking settings.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Cookie.SameParty.BlockedByThirdPartyCookieBlockingSetting"),
              ElementsAre(base::Bucket(/*min=*/1, /*count=*/1)));
}

TEST_F(CookieSettingsTest,
       AnnotateAndMoveUserBlockedCookies_SamePartyConsideredFirstParty) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      net::features::kSamePartyCookiesConsideredFirstParty);
  CookieSettings settings;
  settings.set_block_third_party_cookies(true);

  net::CookieAccessResultList maybe_included_cookies = {
      {*MakeCanonicalCookie("included_third_party", kFPSMemberURL,
                            false /* sameparty */),
       {}},
      {*MakeCanonicalCookie("included_sameparty", kFPSMemberURL,
                            true /* sameparty */),
       {}}};

  // The following exclusion reasons don't make sense when taken together;
  // they're just to exercise the SUT.
  net::CookieAccessResultList excluded_cookies = {
      {*MakeCanonicalCookie("excluded_other", kFPSMemberURL,
                            false /* sameparty */),
       net::CookieAccessResult(net::CookieInclusionStatus(
           net::CookieInclusionStatus::ExclusionReason::EXCLUDE_SECURE_ONLY))},
      {*MakeCanonicalCookie("excluded_invalid_sameparty", kFPSMemberURL,
                            true /* sameparty */),
       net::CookieAccessResult(net::CookieInclusionStatus(
           net::CookieInclusionStatus::ExclusionReason::
               EXCLUDE_SAMEPARTY_CROSS_PARTY_CONTEXT))},
      {*MakeCanonicalCookie("excluded_valid_sameparty", kFPSMemberURL,
                            true /* sameparty */),
       net::CookieAccessResult(net::CookieInclusionStatus(
           net::CookieInclusionStatus::ExclusionReason::EXCLUDE_SECURE_ONLY))}};

  const url::Origin fps_owner_origin = url::Origin::Create(GURL(kFPSOwnerURL));
  EXPECT_TRUE(settings.AnnotateAndMoveUserBlockedCookies(
      GURL(kFPSMemberURL), net::SiteForCookies(), &fps_owner_origin,
      maybe_included_cookies, excluded_cookies));

  EXPECT_THAT(maybe_included_cookies,
              ElementsAre(MatchesCookieWithAccessResult(
                  net::MatchesCookieWithName("included_sameparty"),
                  MatchesCookieAccessResult(net::IsInclude(), _, _, _))));
  EXPECT_THAT(
      excluded_cookies,
      UnorderedElementsAre(
          MatchesCookieWithAccessResult(
              net::MatchesCookieWithName("included_third_party"),
              MatchesCookieAccessResult(
                  HasExactlyExclusionReasonsForTesting(
                      std::vector<net::CookieInclusionStatus::ExclusionReason>{
                          net::CookieInclusionStatus::ExclusionReason::
                              EXCLUDE_USER_PREFERENCES}),
                  _, _, _)),
          MatchesCookieWithAccessResult(
              net::MatchesCookieWithName("excluded_other"),
              MatchesCookieAccessResult(
                  HasExactlyExclusionReasonsForTesting(
                      std::vector<net::CookieInclusionStatus::ExclusionReason>{
                          net::CookieInclusionStatus::ExclusionReason::
                              EXCLUDE_SECURE_ONLY,
                          net::CookieInclusionStatus::ExclusionReason::
                              EXCLUDE_USER_PREFERENCES}),
                  _, _, _)),
          MatchesCookieWithAccessResult(
              net::MatchesCookieWithName("excluded_invalid_sameparty"),
              MatchesCookieAccessResult(
                  HasExactlyExclusionReasonsForTesting(
                      std::vector<net::CookieInclusionStatus::ExclusionReason>{
                          net::CookieInclusionStatus::ExclusionReason::
                              EXCLUDE_SAMEPARTY_CROSS_PARTY_CONTEXT,
                          net::CookieInclusionStatus::ExclusionReason::
                              EXCLUDE_USER_PREFERENCES}),
                  _, _, _)),
          MatchesCookieWithAccessResult(
              net::MatchesCookieWithName("excluded_valid_sameparty"),
              MatchesCookieAccessResult(
                  HasExactlyExclusionReasonsForTesting(
                      std::vector<net::CookieInclusionStatus::ExclusionReason>{
                          net::CookieInclusionStatus::ExclusionReason::
                              EXCLUDE_SECURE_ONLY}),
                  _, _, _))));

  // 2 SameParty cookies were allowed (by user's settings, not by the cookie
  // store), despite 3P cookie blocking being enabled. Note that
  // `excluded_invalid_sameparty` is not in a same-party context, so we do not
  // record metrics for it.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Cookie.SameParty.BlockedByThirdPartyCookieBlockingSetting"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/2)));
}

}  // namespace
}  // namespace network
