// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/browser/page_specific_content_settings.h"

#include <string>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/content_settings/browser/test_page_specific_content_settings_delegate.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/security_state/core/security_state.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_options.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content_settings {
namespace {

class MockSiteDataObserver
    : public PageSpecificContentSettings::SiteDataObserver {
 public:
  explicit MockSiteDataObserver(content::WebContents* web_contents)
      : SiteDataObserver(web_contents) {}

  MockSiteDataObserver(const MockSiteDataObserver&) = delete;
  MockSiteDataObserver& operator=(const MockSiteDataObserver&) = delete;

  ~MockSiteDataObserver() override = default;

  MOCK_METHOD0(OnSiteDataAccessed, void());
};

class MockPageSpecificContentSettingsDelegate
    : public TestPageSpecificContentSettingsDelegate {
 public:
  MockPageSpecificContentSettingsDelegate(PrefService* prefs,
                                          HostContentSettingsMap* settings_map)
      : TestPageSpecificContentSettingsDelegate(prefs, settings_map) {}
  ~MockPageSpecificContentSettingsDelegate() override = default;

  MOCK_METHOD(void, UpdateLocationBar, ());
  MOCK_METHOD(void, OnContentAllowed, (ContentSettingsType type));
  MOCK_METHOD(void, OnContentBlocked, (ContentSettingsType type));
  MOCK_METHOD(void,
              OnCookieAccessAllowed,
              (const net::CookieList& accessed_cookies));
  MOCK_METHOD(void, OnDomStorageAccessAllowed, (const url::Origin& origin));
  MOCK_METHOD(void, OnIndexedDBAccessAllowed, (const url::Origin& origin));
  MOCK_METHOD(void, OnWebDatabaseAccessAllowed, (const url::Origin& origin));
};

}  // namespace

class PageSpecificContentSettingsTest
    : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    HostContentSettingsMap::RegisterProfilePrefs(prefs_.registry());
    settings_map_ = base::MakeRefCounted<HostContentSettingsMap>(
        &prefs_, false /* is_off_the_record */, false /* store_last_modified */,
        false /* restore_session*/);
    PageSpecificContentSettings::CreateForWebContents(
        web_contents(),
        std::make_unique<TestPageSpecificContentSettingsDelegate>(
            &prefs_, settings_map_.get()));
  }

  void TearDown() override {
    settings_map_->ShutdownOnUIThread();
    RenderViewHostTestHarness::TearDown();
  }

  HostContentSettingsMap* settings_map() { return settings_map_.get(); }
  PrefService* prefs() { return &prefs_; }

  content::WebContentsObserver* GetHandle() {
    return PageSpecificContentSettings::GetWebContentsObserverForTest(
        web_contents());
  }

 private:
  sync_preferences::TestingPrefServiceSyncable prefs_;
  scoped_refptr<HostContentSettingsMap> settings_map_;
};

TEST_F(PageSpecificContentSettingsTest, BlockedContent) {
  NavigateAndCommit(GURL("http://google.com"));
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(web_contents()->GetMainFrame());

  // Check that after initializing, nothing is blocked.
#if !defined(OS_ANDROID)
  EXPECT_FALSE(content_settings->IsContentBlocked(ContentSettingsType::IMAGES));
#endif
  EXPECT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::JAVASCRIPT));
  EXPECT_FALSE(content_settings->IsContentBlocked(ContentSettingsType::SOUND));
  EXPECT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::COOKIES));
  EXPECT_FALSE(content_settings->IsContentBlocked(ContentSettingsType::POPUPS));
  EXPECT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_FALSE(content_settings->IsContentBlocked(
      ContentSettingsType::MEDIASTREAM_CAMERA));

  // Set a cookie, block access to images, block mediastream access and block a
  // popup.
  GURL origin("http://google.com");
  std::unique_ptr<net::CanonicalCookie> cookie1(net::CanonicalCookie::Create(
      origin, "A=B", base::Time::Now(), absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */));
  ASSERT_TRUE(cookie1);
  GetHandle()->OnCookiesAccessed(web_contents()->GetMainFrame(),
                                 {content::CookieAccessDetails::Type::kChange,
                                  origin,
                                  origin,
                                  {*cookie1},
                                  false});
  content_settings =
      PageSpecificContentSettings::GetForFrame(web_contents()->GetMainFrame());
#if !defined(OS_ANDROID)
  content_settings->OnContentBlocked(ContentSettingsType::IMAGES);
#endif
  content_settings->OnContentBlocked(ContentSettingsType::POPUPS);
  PageSpecificContentSettings::MicrophoneCameraState
      blocked_microphone_camera_state =
          PageSpecificContentSettings::MICROPHONE_ACCESSED |
          PageSpecificContentSettings::MICROPHONE_BLOCKED |
          PageSpecificContentSettings::CAMERA_ACCESSED |
          PageSpecificContentSettings::CAMERA_BLOCKED;
  content_settings->OnMediaStreamPermissionSet(
      GURL("http://google.com"), blocked_microphone_camera_state, std::string(),
      std::string(), std::string(), std::string());

  // Check that only the respective content types are affected.
#if !defined(OS_ANDROID)
  EXPECT_TRUE(content_settings->IsContentBlocked(ContentSettingsType::IMAGES));
#endif
  EXPECT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::JAVASCRIPT));
  EXPECT_FALSE(content_settings->IsContentBlocked(ContentSettingsType::SOUND));
  EXPECT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::COOKIES));
  EXPECT_TRUE(content_settings->IsContentBlocked(ContentSettingsType::POPUPS));
  EXPECT_TRUE(
      content_settings->IsContentBlocked(ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_TRUE(content_settings->IsContentBlocked(
      ContentSettingsType::MEDIASTREAM_CAMERA));
  GetHandle()->OnCookiesAccessed(web_contents()->GetMainFrame(),
                                 {content::CookieAccessDetails::Type::kChange,
                                  origin,
                                  origin,
                                  {*cookie1},
                                  false});

  // Block a cookie.
  std::unique_ptr<net::CanonicalCookie> cookie2(net::CanonicalCookie::Create(
      origin, "C=D", base::Time::Now(), absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */));
  ASSERT_TRUE(cookie2);
  GetHandle()->OnCookiesAccessed(web_contents()->GetMainFrame(),
                                 {content::CookieAccessDetails::Type::kChange,
                                  origin,
                                  origin,
                                  {*cookie2},
                                  true});
  EXPECT_TRUE(content_settings->IsContentBlocked(ContentSettingsType::COOKIES));

  // Block a javascript during a navigation.
  // Create a pending navigation.
  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateBrowserInitiated(
          GURL("http://google.com"), web_contents());
  simulator->SetTransition(ui::PAGE_TRANSITION_LINK);
  simulator->Start();
  GetHandle()->OnServiceWorkerAccessed(
      simulator->GetNavigationHandle(), GURL("http://google.com"),
      content::AllowServiceWorkerResult::FromPolicy(true, false));
  content_settings =
      PageSpecificContentSettings::GetForFrame(web_contents()->GetMainFrame());
  EXPECT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::JAVASCRIPT));
  simulator->Commit();
  content_settings =
      PageSpecificContentSettings::GetForFrame(web_contents()->GetMainFrame());

  // Block a javascript when page starts to start ServiceWorker.
  GetHandle()->OnServiceWorkerAccessed(
      web_contents()->GetMainFrame(), GURL("http://google.com"),
      content::AllowServiceWorkerResult::FromPolicy(true, false));
  EXPECT_TRUE(
      content_settings->IsContentBlocked(ContentSettingsType::JAVASCRIPT));

  // Reset blocked content settings.
  NavigateAndCommit(GURL("http://google.com"));
  content_settings =
      PageSpecificContentSettings::GetForFrame(web_contents()->GetMainFrame());
#if !defined(OS_ANDROID)
  EXPECT_FALSE(content_settings->IsContentBlocked(ContentSettingsType::IMAGES));
#endif
  EXPECT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::JAVASCRIPT));
  EXPECT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::COOKIES));
  EXPECT_FALSE(content_settings->IsContentBlocked(ContentSettingsType::POPUPS));
  EXPECT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_FALSE(content_settings->IsContentBlocked(
      ContentSettingsType::MEDIASTREAM_CAMERA));
}

TEST_F(PageSpecificContentSettingsTest, BlockedFileSystems) {
  NavigateAndCommit(GURL("http://google.com"));
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(web_contents()->GetMainFrame());

  // Access a file system.
  content_settings->OnFileSystemAccessed(GURL("http://google.com"), false);
  EXPECT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::COOKIES));

  // Block access to a file system.
  content_settings->OnFileSystemAccessed(GURL("http://google.com"), true);
  EXPECT_TRUE(content_settings->IsContentBlocked(ContentSettingsType::COOKIES));
}

TEST_F(PageSpecificContentSettingsTest, AllowedContent) {
  NavigateAndCommit(GURL("http://google.com"));
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(web_contents()->GetMainFrame());

  // Test default settings.
  ASSERT_FALSE(content_settings->IsContentAllowed(ContentSettingsType::IMAGES));
  ASSERT_FALSE(
      content_settings->IsContentAllowed(ContentSettingsType::COOKIES));
  ASSERT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::COOKIES));
  ASSERT_FALSE(
      content_settings->IsContentAllowed(ContentSettingsType::MEDIASTREAM_MIC));
  ASSERT_FALSE(content_settings->IsContentAllowed(
      ContentSettingsType::MEDIASTREAM_CAMERA));

  // Record a cookie.
  GURL origin("http://google.com");
  std::unique_ptr<net::CanonicalCookie> cookie1(net::CanonicalCookie::Create(
      origin, "A=B", base::Time::Now(), absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */));
  ASSERT_TRUE(cookie1);
  GetHandle()->OnCookiesAccessed(web_contents()->GetMainFrame(),
                                 {content::CookieAccessDetails::Type::kChange,
                                  origin,
                                  origin,
                                  {*cookie1},
                                  false});
  ASSERT_TRUE(content_settings->IsContentAllowed(ContentSettingsType::COOKIES));
  ASSERT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::COOKIES));

  // Record a blocked cookie.
  std::unique_ptr<net::CanonicalCookie> cookie2(net::CanonicalCookie::Create(
      origin, "C=D", base::Time::Now(), absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */));
  ASSERT_TRUE(cookie2);
  GetHandle()->OnCookiesAccessed(web_contents()->GetMainFrame(),
                                 {content::CookieAccessDetails::Type::kChange,
                                  origin,
                                  origin,
                                  {*cookie2},
                                  true});
  ASSERT_TRUE(content_settings->IsContentAllowed(ContentSettingsType::COOKIES));
  ASSERT_TRUE(content_settings->IsContentBlocked(ContentSettingsType::COOKIES));
}

TEST_F(PageSpecificContentSettingsTest, EmptyCookieList) {
  NavigateAndCommit(GURL("http://google.com"));
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(web_contents()->GetMainFrame());

  ASSERT_FALSE(
      content_settings->IsContentAllowed(ContentSettingsType::COOKIES));
  ASSERT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::COOKIES));
  GetHandle()->OnCookiesAccessed(
      web_contents()->GetMainFrame(),
      {content::CookieAccessDetails::Type::kRead, GURL("http://google.com"),
       GURL("http://google.com"), net::CookieList(), true});
  ASSERT_FALSE(
      content_settings->IsContentAllowed(ContentSettingsType::COOKIES));
  ASSERT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::COOKIES));
}

TEST_F(PageSpecificContentSettingsTest, SiteDataObserver) {
  NavigateAndCommit(GURL("http://google.com"));
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(web_contents()->GetMainFrame());
  MockSiteDataObserver mock_observer(web_contents());
  EXPECT_CALL(mock_observer, OnSiteDataAccessed()).Times(6);

  bool blocked_by_policy = false;
  GURL origin("http://google.com");
  std::unique_ptr<net::CanonicalCookie> cookie(net::CanonicalCookie::Create(
      origin, "A=B", base::Time::Now(), absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */));
  ASSERT_TRUE(cookie);
  GetHandle()->OnCookiesAccessed(web_contents()->GetMainFrame(),
                                 {content::CookieAccessDetails::Type::kChange,
                                  origin,
                                  origin,
                                  {*cookie},
                                  blocked_by_policy});

  net::CookieList cookie_list;
  std::unique_ptr<net::CanonicalCookie> other_cookie(
      net::CanonicalCookie::Create(GURL("http://google.com"),
                                   "CookieName=CookieValue", base::Time::Now(),
                                   absl::nullopt /* server_time */,
                                   absl::nullopt /* cookie_partition_key */));
  ASSERT_TRUE(other_cookie);

  cookie_list.push_back(*other_cookie);
  GetHandle()->OnCookiesAccessed(
      web_contents()->GetMainFrame(),
      {content::CookieAccessDetails::Type::kRead, GURL("http://google.com"),
       GURL("http://google.com"), cookie_list, blocked_by_policy});
  content_settings->OnFileSystemAccessed(GURL("http://google.com"),
                                         blocked_by_policy);
  content_settings->OnIndexedDBAccessed(GURL("http://google.com"),
                                        blocked_by_policy);
  content_settings->OnDomStorageAccessed(GURL("http://google.com"), true,
                                         blocked_by_policy);
  content_settings->OnWebDatabaseAccessed(GURL("http://google.com"),
                                          blocked_by_policy);
}

TEST_F(PageSpecificContentSettingsTest, LocalSharedObjectsContainer) {
  NavigateAndCommit(GURL("http://google.com"));
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(web_contents()->GetMainFrame());
  bool blocked_by_policy = false;
  auto cookie = net::CanonicalCookie::Create(
      GURL("http://google.com"), "k=v", base::Time::Now(),
      absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */);
  GetHandle()->OnCookiesAccessed(web_contents()->GetMainFrame(),
                                 {content::CookieAccessDetails::Type::kRead,
                                  GURL("http://google.com"),
                                  GURL("http://google.com"),
                                  {*cookie},
                                  blocked_by_policy});
  content_settings->OnFileSystemAccessed(GURL("https://www.google.com"),
                                         blocked_by_policy);
  content_settings->OnIndexedDBAccessed(GURL("https://localhost"),
                                        blocked_by_policy);
  content_settings->OnDomStorageAccessed(GURL("http://maps.google.com:8080"),
                                         true, blocked_by_policy);
  content_settings->OnWebDatabaseAccessed(GURL("http://192.168.0.1"),
                                          blocked_by_policy);
  content_settings->OnSharedWorkerAccessed(
      GURL("http://youtube.com/worker.js"), "worker",
      blink::StorageKey::CreateFromStringForTesting("https://youtube.com"),
      blocked_by_policy);

  const auto& objects = content_settings->allowed_local_shared_objects();
  EXPECT_EQ(6u, objects.GetObjectCount());
  EXPECT_EQ(3u, objects.GetObjectCountForDomain(GURL("http://google.com")));
  EXPECT_EQ(1u, objects.GetObjectCountForDomain(GURL("http://youtube.com")));
  EXPECT_EQ(1u, objects.GetObjectCountForDomain(GURL("http://localhost")));
  EXPECT_EQ(1u, objects.GetObjectCountForDomain(GURL("http://192.168.0.1")));
  // google.com, youtube.com, localhost and 192.168.0.1 should be counted as
  // domains.
  EXPECT_EQ(4u, objects.GetDomainCount());
}

TEST_F(PageSpecificContentSettingsTest, LocalSharedObjectsContainerCookie) {
  NavigateAndCommit(GURL("http://google.com"));
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(web_contents()->GetMainFrame());
  bool blocked_by_policy = false;
  auto cookie1 = net::CanonicalCookie::Create(
      GURL("http://google.com"), "k1=v", base::Time::Now(),
      absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */);
  auto cookie2 = net::CanonicalCookie::Create(
      GURL("http://www.google.com"), "k2=v; Domain=google.com",
      base::Time::Now(), absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */);
  auto cookie3 = net::CanonicalCookie::Create(
      GURL("http://www.google.com"), "k3=v; Domain=.google.com",
      base::Time::Now(), absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */);
  auto cookie4 = net::CanonicalCookie::Create(
      GURL("http://www.google.com"), "k4=v; Domain=.www.google.com",
      base::Time::Now(), absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */);
  GetHandle()->OnCookiesAccessed(web_contents()->GetMainFrame(),
                                 {content::CookieAccessDetails::Type::kRead,
                                  GURL("http://www.google.com"),
                                  GURL("http://www.google.com"),
                                  {*cookie1, *cookie2, *cookie3, *cookie4},
                                  blocked_by_policy});

  auto cookie5 = net::CanonicalCookie::Create(
      GURL("https://www.google.com"), "k5=v", base::Time::Now(),
      absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */);
  GetHandle()->OnCookiesAccessed(web_contents()->GetMainFrame(),
                                 {content::CookieAccessDetails::Type::kRead,
                                  GURL("https://www.google.com"),
                                  GURL("https://www.google.com"),
                                  {*cookie5},
                                  blocked_by_policy});

  const auto& objects = content_settings->allowed_local_shared_objects();
  EXPECT_EQ(5u, objects.GetObjectCount());
  EXPECT_EQ(5u, objects.GetObjectCountForDomain(GURL("http://google.com")));
  EXPECT_EQ(1u, objects.GetDomainCount());
}

TEST_F(PageSpecificContentSettingsTest,
       IndicatorChangedOnContentSettingChange) {
  NavigateAndCommit(GURL("http://google.com"));

  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(web_contents()->GetMainFrame());

  // First trigger OnContentBlocked.
  EXPECT_FALSE(content_settings->IsContentBlocked(
      ContentSettingsType::CLIPBOARD_READ_WRITE));
  content_settings->OnContentBlocked(ContentSettingsType::CLIPBOARD_READ_WRITE);
  EXPECT_TRUE(content_settings->IsContentBlocked(
      ContentSettingsType::CLIPBOARD_READ_WRITE));

  // Simulate the user modifying the setting.
  HostContentSettingsMap* map = settings_map();

  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromURL(web_contents()->GetVisibleURL());

  map->SetWebsiteSettingCustomScope(
      pattern, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::CLIPBOARD_READ_WRITE,
      std::make_unique<base::Value>(CONTENT_SETTING_ALLOW));

  // Now the indicator is set to allowed.
  EXPECT_TRUE(content_settings->IsContentAllowed(
      ContentSettingsType::CLIPBOARD_READ_WRITE));
  EXPECT_FALSE(content_settings->IsContentBlocked(
      ContentSettingsType::CLIPBOARD_READ_WRITE));

  // Simulate the user modifying the setting back to blocked.
  map->SetWebsiteSettingCustomScope(
      pattern, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::CLIPBOARD_READ_WRITE,
      std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));

  // Now the indicator is set to allowed.
  EXPECT_TRUE(content_settings->IsContentBlocked(
      ContentSettingsType::CLIPBOARD_READ_WRITE));
  EXPECT_FALSE(content_settings->IsContentAllowed(
      ContentSettingsType::CLIPBOARD_READ_WRITE));
}

class PageSpecificContentSettingsWithPrerenderTest
    : public PageSpecificContentSettingsTest {
 public:
  PageSpecificContentSettingsWithPrerenderTest() {
    feature_list_.InitWithFeatures(
        {blink::features::kPrerender2},
        // Disable the memory requirement of Prerender2 so the test can run on
        // any bot.
        {blink::features::kPrerender2MemoryControls});
  }
  ~PageSpecificContentSettingsWithPrerenderTest() override = default;

  MockPageSpecificContentSettingsDelegate* InstallMockDelegate() {
    PageSpecificContentSettings::DeleteForWebContentsForTest(web_contents());
    PageSpecificContentSettings::CreateForWebContents(
        web_contents(),
        std::make_unique<
            testing::NiceMock<MockPageSpecificContentSettingsDelegate>>(
            prefs(), settings_map()));
    return static_cast<MockPageSpecificContentSettingsDelegate*>(
        PageSpecificContentSettings::GetDelegateForWebContents(web_contents()));
  }

  content::RenderFrameHost* AddPrerender(const GURL& prerender_url) {
    content::RenderFrameHost* prerender_frame =
        content::WebContentsTester::For(web_contents())
            ->AddPrerenderAndCommitNavigation(prerender_url);
    DCHECK(prerender_frame);
    DCHECK_EQ(prerender_frame->GetLifecycleState(),
              content::RenderFrameHost::LifecycleState::kPrerendering);
    DCHECK_EQ(prerender_frame->GetLastCommittedURL(), prerender_url);
    return prerender_frame;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(PageSpecificContentSettingsWithPrerenderTest, SiteDataAccessed) {
  NavigateAndCommit(GURL("http://google.com"));
  const GURL& prerender_url = GURL("http://google.com/foo");
  content::RenderFrameHost* prerender_frame = AddPrerender(prerender_url);
  PageSpecificContentSettings* pscs =
      PageSpecificContentSettings::GetForFrame(prerender_frame);
  ASSERT_NE(pscs, nullptr);

  // Simulate cookie access.
  {
    MockSiteDataObserver mock_observer(web_contents());
    // OnSiteDataAccessed should not be called for prerendering page.
    EXPECT_CALL(mock_observer, OnSiteDataAccessed()).Times(0);
    // Set a cookie, block access to images, block mediastream access and block
    // a popup.
    GURL origin("http://google.com");
    std::unique_ptr<net::CanonicalCookie> cookie1(net::CanonicalCookie::Create(
        origin, "A=B", base::Time::Now(), absl::nullopt /* server_time */,
        absl::nullopt /* cookie_partition_key */));
    ASSERT_TRUE(cookie1);
    pscs->OnCookiesAccessed({content::CookieAccessDetails::Type::kChange,
                             origin,
                             origin,
                             {*cookie1},
                             false});
  }
  // Activate prerendering page.
  {
    MockSiteDataObserver mock_observer(web_contents());
    // OnSiteDataAccessed should be called after page is activated.
    EXPECT_CALL(mock_observer, OnSiteDataAccessed()).Times(1);
    std::unique_ptr<content::NavigationSimulator> navigation =
        content::NavigationSimulator::CreateRendererInitiated(
            prerender_url, web_contents()->GetMainFrame());
    // TODO(https://crbug.com/1181763): Investigate how default referrer value
    // is set and update here accordingly.
    navigation->SetReferrer(blink::mojom::Referrer::New(
        web_contents()->GetMainFrame()->GetLastCommittedURL(),
        network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin));
    navigation->Commit();
  }
}

TEST_F(PageSpecificContentSettingsWithPrerenderTest,
       DelegateUpdatesSentAfterActivation) {
  MockPageSpecificContentSettingsDelegate* mock_delegate =
      InstallMockDelegate();
  NavigateAndCommit(GURL("http://google.com"));
  const GURL& prerender_url = GURL("http://google.com/foo");
  content::RenderFrameHost* prerender_frame = AddPrerender(prerender_url);
  PageSpecificContentSettings* pscs =
      PageSpecificContentSettings::GetForFrame(prerender_frame);
  ASSERT_NE(pscs, nullptr);

  EXPECT_CALL(*mock_delegate, OnCookieAccessAllowed).Times(0);
  EXPECT_CALL(*mock_delegate, OnIndexedDBAccessAllowed).Times(0);
  EXPECT_CALL(*mock_delegate, OnDomStorageAccessAllowed).Times(0);
  EXPECT_CALL(*mock_delegate, OnWebDatabaseAccessAllowed).Times(0);

  const bool blocked_by_policy = false;
  const GURL url = GURL("http://google.com");
  const url::Origin origin = url::Origin::Create(url);
  auto cookie = net::CanonicalCookie::Create(
      url, "k=v", base::Time::Now(), absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */);
  pscs->OnCookiesAccessed({content::CookieAccessDetails::Type::kRead,
                           url,
                           url,
                           {*cookie},
                           blocked_by_policy});
  pscs->OnIndexedDBAccessed(url, blocked_by_policy);
  pscs->OnDomStorageAccessed(url, true, blocked_by_policy);
  pscs->OnWebDatabaseAccessed(url, blocked_by_policy);

  EXPECT_CALL(*mock_delegate, OnCookieAccessAllowed).Times(1);
  EXPECT_CALL(*mock_delegate, OnIndexedDBAccessAllowed(origin)).Times(1);
  EXPECT_CALL(*mock_delegate, OnDomStorageAccessAllowed(origin)).Times(1);
  EXPECT_CALL(*mock_delegate, OnWebDatabaseAccessAllowed(origin)).Times(1);
  std::unique_ptr<content::NavigationSimulator> navigation =
      content::NavigationSimulator::CreateRendererInitiated(
          prerender_url, web_contents()->GetMainFrame());
  navigation->SetReferrer(blink::mojom::Referrer::New(
      web_contents()->GetMainFrame()->GetLastCommittedURL(),
      network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin));
  navigation->Commit();
}

TEST_F(PageSpecificContentSettingsWithPrerenderTest,
       UpdateLocationBarAfterActivation) {
  MockPageSpecificContentSettingsDelegate* mock_delegate =
      InstallMockDelegate();
  NavigateAndCommit(GURL("http://google.com"));
  const GURL& prerender_url = GURL("http://google.com/foo");
  content::RenderFrameHost* prerender_frame = AddPrerender(prerender_url);
  PageSpecificContentSettings* pscs =
      PageSpecificContentSettings::GetForFrame(prerender_frame);
  ASSERT_NE(pscs, nullptr);

  EXPECT_CALL(*mock_delegate, UpdateLocationBar()).Times(0);
  pscs->OnContentBlocked(ContentSettingsType::JAVASCRIPT);

  EXPECT_CALL(*mock_delegate, UpdateLocationBar()).Times(1);
  std::unique_ptr<content::NavigationSimulator> navigation =
      content::NavigationSimulator::CreateRendererInitiated(
          prerender_url, web_contents()->GetMainFrame());
  navigation->Commit();
}

TEST_F(PageSpecificContentSettingsWithPrerenderTest, ContentAllowedAndBlocked) {
  MockPageSpecificContentSettingsDelegate* mock_delegate =
      InstallMockDelegate();
  NavigateAndCommit(GURL("http://google.com"));
  const GURL& prerender_url = GURL("http://google.com/foo");
  content::RenderFrameHost* prerender_frame = AddPrerender(prerender_url);
  PageSpecificContentSettings* pscs =
      PageSpecificContentSettings::GetForFrame(prerender_frame);
  ASSERT_NE(pscs, nullptr);

  EXPECT_CALL(*mock_delegate, OnContentAllowed).Times(0);
  EXPECT_CALL(*mock_delegate, OnContentBlocked).Times(0);
  pscs->OnContentBlocked(ContentSettingsType::JAVASCRIPT);
  pscs->OnContentAllowed(ContentSettingsType::COOKIES);

  EXPECT_CALL(*mock_delegate, OnContentBlocked(ContentSettingsType::JAVASCRIPT))
      .Times(1);
  EXPECT_CALL(*mock_delegate, OnContentAllowed(ContentSettingsType::COOKIES))
      .Times(1);
  std::unique_ptr<content::NavigationSimulator> navigation =
      content::NavigationSimulator::CreateRendererInitiated(
          prerender_url, web_contents()->GetMainFrame());
  // TODO(https://crbug.com/1181763): Investigate how default referrer value is
  // set and update here accordingly.
  navigation->SetReferrer(blink::mojom::Referrer::New(
      web_contents()->GetMainFrame()->GetLastCommittedURL(),
      network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin));
  navigation->Commit();
}

}  // namespace content_settings
