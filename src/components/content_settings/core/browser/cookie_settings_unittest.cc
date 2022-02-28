// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/cookie_settings.h"

#include <cstddef>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/content_settings/core/test/content_settings_mock_provider.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "extensions/buildflags/buildflags.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if defined(OS_IOS)
#include "components/content_settings/core/common/features.h"
#else
#include "third_party/blink/public/common/features.h"
namespace {
constexpr char kAllowedRequestsHistogram[] =
    "API.StorageAccess.AllowedRequests2";
}
#endif

namespace content_settings {

namespace {

class CookieSettingsObserver : public CookieSettings::Observer {
 public:
  explicit CookieSettingsObserver(CookieSettings* settings)
      : settings_(settings) {
    scoped_observation_.Observe(settings);
  }

  CookieSettingsObserver(const CookieSettingsObserver&) = delete;
  CookieSettingsObserver& operator=(const CookieSettingsObserver&) = delete;

  void OnThirdPartyCookieBlockingChanged(
      bool block_third_party_cookies) override {
    ASSERT_EQ(block_third_party_cookies,
              settings_->ShouldBlockThirdPartyCookies());
    last_value_ = block_third_party_cookies;
  }

  bool last_value() { return last_value_; }

 private:
  raw_ptr<CookieSettings> settings_;
  bool last_value_ = false;
  base::ScopedObservation<CookieSettings, CookieSettings::Observer>
      scoped_observation_{this};
};

class CookieSettingsTest : public testing::Test {
 public:
  CookieSettingsTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        kBlockedSite("http://ads.thirdparty.com"),
        kAllowedSite("http://good.allays.com"),
        kFirstPartySite("http://cool.things.com"),
        kChromeURL("chrome://foo"),
        kExtensionURL("chrome-extension://deadbeef"),
        kDomain("example.com"),
        kDotDomain(".example.com"),
        kSubDomain("www.example.com"),
        kOtherDomain("www.not-example.com"),
        kDomainWildcardPattern("[*.]example.com"),
        kHttpSite("http://example.com"),
        kHttpsSite("https://example.com"),
        kHttpsSubdomainSite("https://www.example.com"),
        kHttpsSite8080("https://example.com:8080"),
        kAllHttpsSitesPattern(ContentSettingsPattern::FromString("https://*")) {
#ifdef OS_IOS
    feature_list_.InitAndEnableFeature(kImprovedCookieControls);
#else
    feature_list_.Init();
#endif
  }

  ~CookieSettingsTest() override { settings_map_->ShutdownOnUIThread(); }

  void SetUp() override {
    ContentSettingsRegistry::GetInstance()->ResetForTest();
    CookieSettings::RegisterProfilePrefs(prefs_.registry());
    HostContentSettingsMap::RegisterProfilePrefs(prefs_.registry());
    settings_map_ = new HostContentSettingsMap(
        &prefs_, false /* is_off_the_record */, false /* store_last_modified */,
        false /* restore_session */);
    cookie_settings_ = new CookieSettings(settings_map_.get(), &prefs_, false,
                                          "chrome-extension");
    cookie_settings_incognito_ = new CookieSettings(
        settings_map_.get(), &prefs_, true, "chrome-extension");
  }

  void FastForwardTime(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

 protected:
  bool ShouldDeleteCookieOnExit(const std::string& domain, bool is_https) {
    ContentSettingsForOneType settings;
    cookie_settings_->GetCookieSettings(&settings);
    return cookie_settings_->ShouldDeleteCookieOnExit(settings, domain,
                                                      is_https);
  }

  // There must be a valid ThreadTaskRunnerHandle in HostContentSettingsMap's
  // scope.
  base::test::SingleThreadTaskEnvironment task_environment_;

  sync_preferences::TestingPrefServiceSyncable prefs_;
  scoped_refptr<HostContentSettingsMap> settings_map_;
  scoped_refptr<CookieSettings> cookie_settings_;
  scoped_refptr<CookieSettings> cookie_settings_incognito_;
  const GURL kBlockedSite;
  const GURL kAllowedSite;
  const GURL kFirstPartySite;
  const GURL kChromeURL;
  const GURL kExtensionURL;
  const std::string kDomain;
  const std::string kDotDomain;
  const std::string kSubDomain;
  const std::string kOtherDomain;
  const std::string kDomainWildcardPattern;
  const GURL kHttpSite;
  const GURL kHttpsSite;
  const GURL kHttpsSubdomainSite;
  const GURL kHttpsSite8080;
  ContentSettingsPattern kAllHttpsSitesPattern;

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(CookieSettingsTest, TestAllowlistedScheme) {
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(
      cookie_settings_->IsFullCookieAccessAllowed(kHttpSite, kChromeURL));
  EXPECT_TRUE(
      cookie_settings_->IsFullCookieAccessAllowed(kHttpsSite, kChromeURL));
  EXPECT_TRUE(
      cookie_settings_->IsFullCookieAccessAllowed(kChromeURL, kHttpSite));
#if BUILDFLAG(ENABLE_EXTENSIONS)
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(kExtensionURL,
                                                          kExtensionURL));
#else
  EXPECT_FALSE(cookie_settings_->IsFullCookieAccessAllowed(kExtensionURL,
                                                           kExtensionURL));
#endif
  EXPECT_FALSE(
      cookie_settings_->IsFullCookieAccessAllowed(kExtensionURL, kHttpSite));
}

TEST_F(CookieSettingsTest, CookiesBlockSingle) {
  cookie_settings_->SetCookieSetting(kBlockedSite, CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(
      cookie_settings_->IsFullCookieAccessAllowed(kBlockedSite, kBlockedSite));
}

TEST_F(CookieSettingsTest, CookiesBlockThirdParty) {
  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));
  EXPECT_FALSE(cookie_settings_->IsFullCookieAccessAllowed(kBlockedSite,
                                                           kFirstPartySite));
  EXPECT_FALSE(cookie_settings_->IsCookieSessionOnly(kBlockedSite));
}

TEST_F(CookieSettingsTest, CookiesControlsDefault) {
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(kBlockedSite,
                                                          kFirstPartySite));
  EXPECT_FALSE(cookie_settings_incognito_->IsFullCookieAccessAllowed(
      kBlockedSite, kFirstPartySite));
}

TEST_F(CookieSettingsTest, CookiesControlsEnabled) {
  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));
  EXPECT_FALSE(cookie_settings_->IsFullCookieAccessAllowed(kBlockedSite,
                                                           kFirstPartySite));
  EXPECT_FALSE(cookie_settings_incognito_->IsFullCookieAccessAllowed(
      kBlockedSite, kFirstPartySite));
}

TEST_F(CookieSettingsTest, CookiesControlsDisabled) {
  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kOff));
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(kBlockedSite,
                                                          kFirstPartySite));
  EXPECT_TRUE(cookie_settings_incognito_->IsFullCookieAccessAllowed(
      kBlockedSite, kFirstPartySite));
}

TEST_F(CookieSettingsTest, CookiesControlsEnabledForIncognito) {
  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kIncognitoOnly));
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(kBlockedSite,
                                                          kFirstPartySite));
  EXPECT_FALSE(cookie_settings_incognito_->IsFullCookieAccessAllowed(
      kBlockedSite, kFirstPartySite));
}

#if defined(OS_IOS)
// Test fixture with ImprovedCookieControls disabled.
class ImprovedCookieControlsDisabledCookieSettingsTest
    : public CookieSettingsTest {
 public:
  ImprovedCookieControlsDisabledCookieSettingsTest() : CookieSettingsTest() {
    feature_list_.InitAndDisableFeature(kImprovedCookieControls);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ImprovedCookieControlsDisabledCookieSettingsTest,
       CookiesControlsEnabledButFeatureDisabled) {
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(kBlockedSite,
                                                          kFirstPartySite));
  EXPECT_TRUE(cookie_settings_incognito_->IsFullCookieAccessAllowed(
      kBlockedSite, kFirstPartySite));
  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(kBlockedSite,
                                                          kFirstPartySite));
  EXPECT_TRUE(cookie_settings_incognito_->IsFullCookieAccessAllowed(
      kBlockedSite, kFirstPartySite));
}
#endif

TEST_F(CookieSettingsTest, CookiesAllowThirdParty) {
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(kBlockedSite,
                                                          kFirstPartySite));
  EXPECT_FALSE(cookie_settings_->IsCookieSessionOnly(kBlockedSite));
}

TEST_F(CookieSettingsTest, CookiesExplicitBlockSingleThirdParty) {
  cookie_settings_->SetCookieSetting(kBlockedSite, CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(cookie_settings_->IsFullCookieAccessAllowed(kBlockedSite,
                                                           kFirstPartySite));
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(kAllowedSite,
                                                          kFirstPartySite));
}

TEST_F(CookieSettingsTest, CookiesExplicitSessionOnly) {
  cookie_settings_->SetCookieSetting(kBlockedSite,
                                     CONTENT_SETTING_SESSION_ONLY);
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(kBlockedSite,
                                                          kFirstPartySite));
  EXPECT_TRUE(cookie_settings_->IsCookieSessionOnly(kBlockedSite));

  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(kBlockedSite,
                                                          kFirstPartySite));
  EXPECT_TRUE(cookie_settings_->IsCookieSessionOnly(kBlockedSite));
}

TEST_F(CookieSettingsTest, KeepBlocked) {
  // Keep blocked cookies.
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_ALLOW);
  cookie_settings_->SetCookieSetting(kHttpsSite, CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDomain, false));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDomain, true));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDotDomain, false));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDotDomain, true));
}

TEST_F(CookieSettingsTest, DeleteSessionOnly) {
  // Keep session_only http cookies if https is allowed.
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);
  cookie_settings_->SetCookieSetting(kHttpsSite, CONTENT_SETTING_ALLOW);
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDomain, false));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDomain, true));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDotDomain, false));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDotDomain, true));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kSubDomain, false));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kSubDomain, true));

  // Delete cookies if site is session only.
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  cookie_settings_->SetCookieSetting(kHttpsSite, CONTENT_SETTING_SESSION_ONLY);
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDomain, false));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDomain, true));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDotDomain, false));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDotDomain, true));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kSubDomain, false));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kSubDomain, true));

  // Http blocked, https allowed - keep secure and non secure cookies.
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);
  cookie_settings_->SetCookieSetting(kHttpSite, CONTENT_SETTING_BLOCK);
  cookie_settings_->SetCookieSetting(kHttpsSite, CONTENT_SETTING_ALLOW);
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDomain, false));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDomain, true));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDotDomain, false));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDotDomain, true));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kSubDomain, false));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kSubDomain, true));

  // Http and https session only, all is deleted.
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_ALLOW);
  cookie_settings_->SetCookieSetting(kHttpSite, CONTENT_SETTING_SESSION_ONLY);
  cookie_settings_->SetCookieSetting(kHttpsSite, CONTENT_SETTING_SESSION_ONLY);
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDomain, false));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDomain, true));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDotDomain, false));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDotDomain, true));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kSubDomain, false));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kSubDomain, true));
}

TEST_F(CookieSettingsTest, DeletionWithDifferentPorts) {
  // Keep cookies for site with special port.
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);
  cookie_settings_->SetCookieSetting(kHttpsSite8080, CONTENT_SETTING_ALLOW);
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDomain, false));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDomain, true));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDotDomain, false));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDotDomain, true));

  // Delete cookies with special port.
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  cookie_settings_->SetCookieSetting(kHttpsSite8080,
                                     CONTENT_SETTING_SESSION_ONLY);
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDomain, false));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDomain, true));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDotDomain, false));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDotDomain, true));
}

TEST_F(CookieSettingsTest, DeletionWithSubDomains) {
  // Cookies accessible by subdomains are kept.
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);
  cookie_settings_->SetCookieSetting(kHttpsSubdomainSite,
                                     CONTENT_SETTING_ALLOW);
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDotDomain, false));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDotDomain, true));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDomain, false));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDomain, true));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kSubDomain, false));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kSubDomain, true));

  // Cookies that have a session_only subdomain but are accessible by allowed
  // domains are kept.
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_ALLOW);
  cookie_settings_->SetCookieSetting(kHttpsSubdomainSite,
                                     CONTENT_SETTING_SESSION_ONLY);
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDotDomain, false));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDotDomain, true));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDomain, false));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDomain, true));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kSubDomain, false));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kSubDomain, true));

  // Cookies created by session_only subdomains are deleted.
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  cookie_settings_->SetCookieSetting(kHttpsSubdomainSite,
                                     CONTENT_SETTING_SESSION_ONLY);
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDotDomain, false));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDotDomain, true));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDomain, false));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDomain, true));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kSubDomain, false));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kSubDomain, true));
}

TEST_F(CookieSettingsTest, CookiesThirdPartyBlockedExplicitAllow) {
  cookie_settings_->SetCookieSetting(kAllowedSite, CONTENT_SETTING_ALLOW);
  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(kAllowedSite,
                                                          kFirstPartySite));
  EXPECT_FALSE(cookie_settings_->IsCookieSessionOnly(kAllowedSite));

  // Extensions should always be allowed to use cookies.
  EXPECT_TRUE(
      cookie_settings_->IsFullCookieAccessAllowed(kAllowedSite, kExtensionURL));
}

TEST_F(CookieSettingsTest, CookiesThirdPartyBlockedAllSitesAllowed) {
  cookie_settings_->SetCookieSetting(kAllowedSite, CONTENT_SETTING_ALLOW);
  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));
  // As an example for a url that matches all hosts but not all origins,
  // match all HTTPS sites.
  settings_map_->SetContentSettingCustomScope(
      kAllHttpsSitesPattern, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::COOKIES, CONTENT_SETTING_ALLOW);
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);

  // |kAllowedSite| should be allowed.
  EXPECT_TRUE(
      cookie_settings_->IsFullCookieAccessAllowed(kAllowedSite, kBlockedSite));
  EXPECT_FALSE(cookie_settings_->IsCookieSessionOnly(kAllowedSite));

  // HTTPS sites should be allowed in a first-party context.
  EXPECT_TRUE(
      cookie_settings_->IsFullCookieAccessAllowed(kHttpsSite, kHttpsSite));
  EXPECT_FALSE(cookie_settings_->IsCookieSessionOnly(kAllowedSite));

  // HTTP sites should be allowed, but session-only.
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(kFirstPartySite,
                                                          kFirstPartySite));
  EXPECT_TRUE(cookie_settings_->IsCookieSessionOnly(kFirstPartySite));

  // Third-party cookies should be blocked.
  EXPECT_FALSE(cookie_settings_->IsFullCookieAccessAllowed(kFirstPartySite,
                                                           kBlockedSite));
  EXPECT_FALSE(
      cookie_settings_->IsFullCookieAccessAllowed(kHttpsSite, kBlockedSite));
}

TEST_F(CookieSettingsTest, CookiesBlockEverything) {
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

  EXPECT_FALSE(cookie_settings_->IsFullCookieAccessAllowed(kFirstPartySite,
                                                           kFirstPartySite));
  EXPECT_FALSE(cookie_settings_->IsFullCookieAccessAllowed(kAllowedSite,
                                                           kFirstPartySite));
}

TEST_F(CookieSettingsTest, CookiesBlockEverythingExceptAllowed) {
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  cookie_settings_->SetCookieSetting(kAllowedSite, CONTENT_SETTING_ALLOW);
  EXPECT_FALSE(cookie_settings_->IsFullCookieAccessAllowed(kFirstPartySite,
                                                           kFirstPartySite));
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(kAllowedSite,
                                                          kFirstPartySite));
  EXPECT_TRUE(
      cookie_settings_->IsFullCookieAccessAllowed(kAllowedSite, kAllowedSite));
  EXPECT_FALSE(cookie_settings_->IsCookieSessionOnly(kAllowedSite));
}

#if !defined(OS_IOS)
TEST_F(CookieSettingsTest, GetCookieSettingAllowedTelemetry) {
  const GURL top_level_url = GURL(kFirstPartySite);
  const GURL url = GURL(kAllowedSite);

  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kOff));

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 0);

  EXPECT_EQ(cookie_settings_->GetCookieSetting(url, top_level_url, nullptr),
            CONTENT_SETTING_ALLOW);
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 1);
  histogram_tester.ExpectBucketCount(
      kAllowedRequestsHistogram,
      static_cast<int>(net::cookie_util::StorageAccessResult::ACCESS_ALLOWED),
      1);
}

// When the Storage Access API is disabled, block third party cookie setting
// should behave like normal.
TEST_F(CookieSettingsTest, GetCookieSettingDisabledSAA) {
  base::test::ScopedFeatureList scoped_disable;
  scoped_disable.InitAndDisableFeature(blink::features::kStorageAccessAPI);

  const GURL top_level_url = GURL(kFirstPartySite);
  const GURL url = GURL(kAllowedSite);

  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));

  settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(url),
      ContentSettingsPattern::FromURLNoWildcard(top_level_url),
      ContentSettingsType::STORAGE_ACCESS, CONTENT_SETTING_ALLOW);

  EXPECT_EQ(cookie_settings_->GetCookieSetting(url, top_level_url, nullptr),
            CONTENT_SETTING_BLOCK);
}

// The current default behaviour of the Storage Access API should be to not
// enable access as it is gated behind |kStorageAccessAPI|.
TEST_F(CookieSettingsTest, GetCookieSettingDefaultSAA) {
  const GURL top_level_url = GURL(kFirstPartySite);
  const GURL url = GURL(kAllowedSite);

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 0);

  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));

  settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(url),
      ContentSettingsPattern::FromURLNoWildcard(top_level_url),
      ContentSettingsType::STORAGE_ACCESS, CONTENT_SETTING_ALLOW);

  EXPECT_EQ(cookie_settings_->GetCookieSetting(url, top_level_url, nullptr),
            CONTENT_SETTING_BLOCK);
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 1);
  histogram_tester.ExpectBucketCount(
      kAllowedRequestsHistogram,
      static_cast<int>(net::cookie_util::StorageAccessResult::ACCESS_BLOCKED),
      1);
}

// When enabled, the Storage Access API should unblock storage access that would
// otherwise be blocked.
TEST_F(CookieSettingsTest, GetCookieSettingEnabledSAA) {
  base::test::ScopedFeatureList scoped_enable;
  scoped_enable.InitAndEnableFeature(blink::features::kStorageAccessAPI);

  const GURL top_level_url = GURL(kFirstPartySite);
  const GURL url = GURL(kAllowedSite);
  const GURL third_url = GURL(kBlockedSite);

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 0);

  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));

  settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(url),
      ContentSettingsPattern::FromURLNoWildcard(top_level_url),
      ContentSettingsType::STORAGE_ACCESS, CONTENT_SETTING_ALLOW);

  // When requesting our setting for the url/top-level combination our
  // grant is for access should be allowed. For any other domain pairs access
  // should still be blocked.
  EXPECT_EQ(cookie_settings_->GetCookieSetting(url, top_level_url, nullptr),
            CONTENT_SETTING_ALLOW);
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 1);
  histogram_tester.ExpectBucketCount(
      kAllowedRequestsHistogram,
      static_cast<int>(net::cookie_util::StorageAccessResult::
                           ACCESS_ALLOWED_STORAGE_ACCESS_GRANT),
      1);

  // Invalid pair the |top_level_url| granting access to |url| is now
  // being loaded under |url| as the top level url.
  EXPECT_EQ(cookie_settings_->GetCookieSetting(top_level_url, url, nullptr),
            CONTENT_SETTING_BLOCK);

  // Invalid pairs where a |third_url| is used.
  EXPECT_EQ(cookie_settings_->GetCookieSetting(url, third_url, nullptr),
            CONTENT_SETTING_BLOCK);
  EXPECT_EQ(
      cookie_settings_->GetCookieSetting(third_url, top_level_url, nullptr),
      CONTENT_SETTING_BLOCK);
}

// Subdomains of the granted resource url should not gain access if a valid
// grant exists.
TEST_F(CookieSettingsTest, GetCookieSettingSAAResourceWildcards) {
  base::test::ScopedFeatureList scoped_enable;
  scoped_enable.InitAndEnableFeature(blink::features::kStorageAccessAPI);

  const GURL top_level_url = GURL(kFirstPartySite);
  const GURL url = GURL(kHttpSite);

  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));

  settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(url),
      ContentSettingsPattern::FromURLNoWildcard(top_level_url),
      ContentSettingsType::STORAGE_ACCESS, CONTENT_SETTING_ALLOW);

  EXPECT_EQ(cookie_settings_->GetCookieSetting(url, top_level_url, nullptr),
            CONTENT_SETTING_ALLOW);
  EXPECT_EQ(cookie_settings_->GetCookieSetting(GURL(kHttpsSubdomainSite),
                                               top_level_url, nullptr),
            CONTENT_SETTING_BLOCK);
}

// Subdomains of the granted top level url should not grant access if a valid
// grant exists.
TEST_F(CookieSettingsTest, GetCookieSettingSAATopLevelWildcards) {
  base::test::ScopedFeatureList scoped_enable;
  scoped_enable.InitAndEnableFeature(blink::features::kStorageAccessAPI);

  const GURL top_level_url = GURL(kHttpSite);
  const GURL url = GURL(kFirstPartySite);

  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));

  settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(url),
      ContentSettingsPattern::FromURLNoWildcard(top_level_url),
      ContentSettingsType::STORAGE_ACCESS, CONTENT_SETTING_ALLOW);

  EXPECT_EQ(cookie_settings_->GetCookieSetting(url, top_level_url, nullptr),
            CONTENT_SETTING_ALLOW);
  EXPECT_EQ(cookie_settings_->GetCookieSetting(url, GURL(kHttpsSubdomainSite),
                                               nullptr),
            CONTENT_SETTING_BLOCK);
}

// Any Storage Access API grant should not override an explicit setting to block
// cookie access.
TEST_F(CookieSettingsTest, GetCookieSettingSAARespectsSettings) {
  base::test::ScopedFeatureList scoped_enable;
  scoped_enable.InitAndEnableFeature(blink::features::kStorageAccessAPI);

  const GURL top_level_url = GURL(kFirstPartySite);
  const GURL url = GURL(kAllowedSite);

  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

  settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(url),
      ContentSettingsPattern::FromURLNoWildcard(top_level_url),
      ContentSettingsType::STORAGE_ACCESS, CONTENT_SETTING_ALLOW);

  EXPECT_EQ(cookie_settings_->GetCookieSetting(url, top_level_url, nullptr),
            CONTENT_SETTING_BLOCK);
}

// Once a grant expires access should no longer be given.
TEST_F(CookieSettingsTest, GetCookieSettingSAAExpiredGrant) {
  base::test::ScopedFeatureList scoped_enable;
  scoped_enable.InitAndEnableFeature(blink::features::kStorageAccessAPI);

  const GURL top_level_url = GURL(kFirstPartySite);
  const GURL url = GURL(kAllowedSite);

  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));

  settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(url),
      ContentSettingsPattern::FromURLNoWildcard(top_level_url),
      ContentSettingsType::STORAGE_ACCESS, CONTENT_SETTING_ALLOW,
      {content_settings::GetConstraintExpiration(base::Seconds(100)),
       SessionModel::UserSession});

  // When requesting our setting for the url/top-level combination our
  // grant is for access should be allowed. For any other domain pairs access
  // should still be blocked.
  EXPECT_EQ(cookie_settings_->GetCookieSetting(url, top_level_url, nullptr),
            CONTENT_SETTING_ALLOW);

  // If we fastforward past the expiration of our grant the result should be
  // CONTENT_SETTING_BLOCK now.
  FastForwardTime(base::Seconds(101));
  EXPECT_EQ(cookie_settings_->GetCookieSetting(url, top_level_url, nullptr),
            CONTENT_SETTING_BLOCK);
}
#endif

TEST_F(CookieSettingsTest, ExtensionsRegularSettings) {
  cookie_settings_->SetCookieSetting(kBlockedSite, CONTENT_SETTING_BLOCK);

  // Regular cookie settings also apply to extensions.
  EXPECT_FALSE(
      cookie_settings_->IsFullCookieAccessAllowed(kBlockedSite, kExtensionURL));
}

TEST_F(CookieSettingsTest, ExtensionsOwnCookies) {
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Extensions can always use cookies (and site data) in their own origin.
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(kExtensionURL,
                                                          kExtensionURL));
#else
  // Except if extensions are disabled. Then the extension-specific checks do
  // not exist and the default setting is to block.
  EXPECT_FALSE(cookie_settings_->IsFullCookieAccessAllowed(kExtensionURL,
                                                           kExtensionURL));
#endif
}

TEST_F(CookieSettingsTest, ExtensionsThirdParty) {
  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));

  // XHRs stemming from extensions are exempt from third-party cookie blocking
  // rules (as the first party is always the extension's security origin).
  EXPECT_TRUE(
      cookie_settings_->IsFullCookieAccessAllowed(kBlockedSite, kExtensionURL));
}

TEST_F(CookieSettingsTest, ThirdPartyException) {
  EXPECT_TRUE(
      cookie_settings_->IsThirdPartyAccessAllowed(kFirstPartySite, nullptr));
  EXPECT_TRUE(
      cookie_settings_->IsFullCookieAccessAllowed(kHttpsSite, kFirstPartySite));

  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));
  EXPECT_FALSE(
      cookie_settings_->IsThirdPartyAccessAllowed(kFirstPartySite, nullptr));
  EXPECT_FALSE(
      cookie_settings_->IsFullCookieAccessAllowed(kHttpsSite, kFirstPartySite));

  cookie_settings_->SetThirdPartyCookieSetting(kFirstPartySite,
                                               CONTENT_SETTING_ALLOW);
  EXPECT_TRUE(
      cookie_settings_->IsThirdPartyAccessAllowed(kFirstPartySite, nullptr));
  EXPECT_TRUE(
      cookie_settings_->IsFullCookieAccessAllowed(kHttpsSite, kFirstPartySite));

  cookie_settings_->ResetThirdPartyCookieSetting(kFirstPartySite);
  EXPECT_FALSE(
      cookie_settings_->IsThirdPartyAccessAllowed(kFirstPartySite, nullptr));
  EXPECT_FALSE(
      cookie_settings_->IsFullCookieAccessAllowed(kHttpsSite, kFirstPartySite));

  cookie_settings_->SetCookieSetting(kHttpsSite, CONTENT_SETTING_ALLOW);
  EXPECT_FALSE(
      cookie_settings_->IsThirdPartyAccessAllowed(kFirstPartySite, nullptr));
  EXPECT_TRUE(
      cookie_settings_->IsFullCookieAccessAllowed(kHttpsSite, kFirstPartySite));
}

TEST_F(CookieSettingsTest, ManagedThirdPartyException) {
  SettingSource source;
  EXPECT_TRUE(
      cookie_settings_->IsThirdPartyAccessAllowed(kFirstPartySite, &source));
  EXPECT_TRUE(
      cookie_settings_->IsFullCookieAccessAllowed(kHttpsSite, kFirstPartySite));
  EXPECT_EQ(source, SettingSource::SETTING_SOURCE_USER);

  prefs_.SetManagedPref(prefs::kManagedDefaultCookiesSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));
  EXPECT_FALSE(
      cookie_settings_->IsThirdPartyAccessAllowed(kFirstPartySite, &source));
  EXPECT_FALSE(
      cookie_settings_->IsFullCookieAccessAllowed(kHttpsSite, kFirstPartySite));
  EXPECT_EQ(source, SettingSource::SETTING_SOURCE_POLICY);
}

TEST_F(CookieSettingsTest, ThirdPartySettingObserver) {
  CookieSettingsObserver observer(cookie_settings_.get());
  EXPECT_FALSE(observer.last_value());
  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));
  EXPECT_TRUE(observer.last_value());
}

TEST_F(CookieSettingsTest, LegacyCookieAccessAllowAll) {
  settings_map_->SetDefaultContentSetting(
      ContentSettingsType::LEGACY_COOKIE_ACCESS, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(net::CookieAccessSemantics::LEGACY,
            cookie_settings_->GetCookieAccessSemanticsForDomain(kDomain));
  EXPECT_EQ(net::CookieAccessSemantics::LEGACY,
            cookie_settings_->GetCookieAccessSemanticsForDomain(kDotDomain));
}

TEST_F(CookieSettingsTest, LegacyCookieAccessBlockAll) {
  settings_map_->SetDefaultContentSetting(
      ContentSettingsType::LEGACY_COOKIE_ACCESS, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(net::CookieAccessSemantics::NONLEGACY,
            cookie_settings_->GetCookieAccessSemanticsForDomain(kDomain));
  EXPECT_EQ(net::CookieAccessSemantics::NONLEGACY,
            cookie_settings_->GetCookieAccessSemanticsForDomain(kDotDomain));
}

TEST_F(CookieSettingsTest, LegacyCookieAccessAllowDomainPattern) {
  // Override the policy provider for this test, since the legacy cookie access
  // setting can only be set by policy.
  TestUtils::OverrideProvider(
      settings_map_.get(), std::make_unique<MockProvider>(),
      HostContentSettingsMap::ProviderType::POLICY_PROVIDER);
  settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::FromString(kDomain),
      ContentSettingsPattern::Wildcard(),
      ContentSettingsType::LEGACY_COOKIE_ACCESS, CONTENT_SETTING_ALLOW);
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
    EXPECT_EQ(test.status, cookie_settings_->GetCookieAccessSemanticsForDomain(
                               test.cookie_domain));
  }
}

TEST_F(CookieSettingsTest, LegacyCookieAccessAllowDomainWildcardPattern) {
  // Override the policy provider for this test, since the legacy cookie access
  // setting can only be set by policy.
  TestUtils::OverrideProvider(
      settings_map_.get(), std::make_unique<MockProvider>(),
      HostContentSettingsMap::ProviderType::POLICY_PROVIDER);
  settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::FromString(kDomainWildcardPattern),
      ContentSettingsPattern::Wildcard(),
      ContentSettingsType::LEGACY_COOKIE_ACCESS, CONTENT_SETTING_ALLOW);
  const struct {
    net::CookieAccessSemantics status;
    std::string cookie_domain;
  } kTestCases[] = {
      // These three test cases are LEGACY because they match the setting.
      {net::CookieAccessSemantics::LEGACY, kDomain},
      {net::CookieAccessSemantics::LEGACY, kDotDomain},
      // Subdomain matches pattern.
      {net::CookieAccessSemantics::LEGACY, kSubDomain},
      // This test case defaults into NONLEGACY.
      {net::CookieAccessSemantics::NONLEGACY, kOtherDomain}};
  for (const auto& test : kTestCases) {
    EXPECT_EQ(test.status, cookie_settings_->GetCookieAccessSemanticsForDomain(
                               test.cookie_domain));
  }
}

}  // namespace

}  // namespace content_settings
