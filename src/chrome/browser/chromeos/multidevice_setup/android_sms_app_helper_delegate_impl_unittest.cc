// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/multidevice_setup/android_sms_app_helper_delegate_impl.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/chromeos/android_sms/android_sms_urls.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/web_applications/components/test_pending_app_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class FakeCookieManager : public network::mojom::CookieManager {
 public:
  const std::vector<std::tuple<net::CanonicalCookie, bool, bool>>&
  set_canonical_cookie_calls() {
    return set_canonical_cookie_calls_;
  }

  const std::vector<network::mojom::CookieDeletionFilterPtr>&
  delete_cookies_calls() {
    return delete_cookies_calls_;
  }

  // network::mojom::CookieManager
  void SetCanonicalCookie(const net::CanonicalCookie& cookie,
                          bool secure_source,
                          bool modify_http_only,
                          SetCanonicalCookieCallback callback) override {
    set_canonical_cookie_calls_.push_back(
        std::make_tuple(cookie, secure_source, modify_http_only));
    std::move(callback).Run(true);
  }

  void DeleteCookies(network::mojom::CookieDeletionFilterPtr filter,
                     DeleteCookiesCallback callback) override {
    delete_cookies_calls_.push_back(std::move(filter));
    std::move(callback).Run(1);
  }

  void GetAllCookies(GetAllCookiesCallback callback) override {}
  void GetCookieList(const GURL& url,
                     const net::CookieOptions& cookie_options,
                     GetCookieListCallback callback) override {}
  void DeleteCanonicalCookie(const net::CanonicalCookie& cookie,
                             DeleteCanonicalCookieCallback callback) override {}
  void AddCookieChangeListener(
      const GURL& url,
      const std::string& name,
      network::mojom::CookieChangeListenerPtr listener) override {}
  void AddGlobalChangeListener(
      network::mojom::CookieChangeListenerPtr notification_pointer) override {}
  void CloneInterface(
      network::mojom::CookieManagerRequest new_interface) override {}
  void FlushCookieStore(FlushCookieStoreCallback callback) override {}
  void SetContentSettings(
      const std::vector<::ContentSettingPatternSource>& settings) override {}
  void SetForceKeepSessionState() override {}
  void BlockThirdPartyCookies(bool block) override {}

 private:
  std::vector<std::tuple<net::CanonicalCookie, bool, bool>>
      set_canonical_cookie_calls_;
  std::vector<network::mojom::CookieDeletionFilterPtr> delete_cookies_calls_;
};
}  // namespace

namespace chromeos {

namespace multidevice_setup {

class AndroidSmsAppHelperDelegateImplTest : public testing::Test {
 protected:
  AndroidSmsAppHelperDelegateImplTest()
      : host_content_settings_map_(
            HostContentSettingsMapFactory::GetForProfile(&profile_)) {}

  ~AndroidSmsAppHelperDelegateImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    host_content_settings_map_->ClearSettingsForOneType(
        ContentSettingsType::CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
    test_pending_app_manager_ =
        std::make_unique<web_app::TestPendingAppManager>();
    fake_cookie_manager_ = std::make_unique<FakeCookieManager>();
    android_sms_app_helper_delegate_ =
        base::WrapUnique(new AndroidSmsAppHelperDelegateImpl(
            test_pending_app_manager_.get(), host_content_settings_map_,
            fake_cookie_manager_.get()));
  }

  web_app::TestPendingAppManager* test_pending_app_manager() {
    return test_pending_app_manager_.get();
  }

  FakeCookieManager* fake_cookie_manager() {
    return fake_cookie_manager_.get();
  }

  void SetUpApp() { android_sms_app_helper_delegate_->SetUpAndroidSmsApp(); }

  void SetUpAndLaunchApp() {
    android_sms_app_helper_delegate_->SetUpAndLaunchAndroidSmsApp();
  }

  void TearDownApp() {
    android_sms_app_helper_delegate_->TearDownAndroidSmsApp();
  }

  ContentSetting GetNotificationSetting() {
    std::unique_ptr<base::Value> notification_settings_value =
        host_content_settings_map_->GetWebsiteSetting(
            chromeos::android_sms::GetAndroidMessagesURL(),
            GURL() /* top_level_url */,
            ContentSettingsType::CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
            content_settings::ResourceIdentifier(), nullptr);
    return static_cast<ContentSetting>(notification_settings_value->GetInt());
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  HostContentSettingsMap* host_content_settings_map_;
  std::unique_ptr<FakeCookieManager> fake_cookie_manager_;
  std::unique_ptr<web_app::TestPendingAppManager> test_pending_app_manager_;
  std::unique_ptr<AndroidSmsAppHelperDelegate> android_sms_app_helper_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AndroidSmsAppHelperDelegateImplTest);
};

TEST_F(AndroidSmsAppHelperDelegateImplTest, TestInstallMessagesApp) {
  base::HistogramTester histogram_tester;
  EXPECT_NE(ContentSetting::CONTENT_SETTING_ALLOW, GetNotificationSetting());
  SetUpApp();

  std::vector<web_app::PendingAppManager::AppInfo> expected_apps_to_install;

  web_app::PendingAppManager::AppInfo info(
      chromeos::android_sms::GetAndroidMessagesURLWithParams(),
      web_app::LaunchContainer::kWindow, web_app::InstallSource::kInternal);
  info.override_previous_user_uninstall = true;
  info.bypass_service_worker_check = true;
  info.require_manifest = true;

  expected_apps_to_install.push_back(std::move(info));

  EXPECT_EQ(expected_apps_to_install,
            test_pending_app_manager()->install_requests());
  EXPECT_EQ(ContentSetting::CONTENT_SETTING_ALLOW, GetNotificationSetting());
  histogram_tester.ExpectBucketCount("AndroidSms.PWAInstallationResult",
                                     web_app::InstallResultCode::kSuccess, 1);

  // Check if the default_to_persist cookie is set.
  ASSERT_EQ(1u, fake_cookie_manager()->set_canonical_cookie_calls().size());
  std::tuple<net::CanonicalCookie, bool, bool> set_cookie_call =
      fake_cookie_manager()->set_canonical_cookie_calls()[0];
  EXPECT_EQ("default_to_persist", std::get<0>(set_cookie_call).Name());
  EXPECT_EQ("true", std::get<0>(set_cookie_call).Value());
  EXPECT_TRUE(std::get<1>(set_cookie_call));
  EXPECT_FALSE(std::get<2>(set_cookie_call));
}

TEST_F(AndroidSmsAppHelperDelegateImplTest, TestTearDownMessagesApp) {
  TearDownApp();
  ASSERT_EQ(1u, fake_cookie_manager()->delete_cookies_calls().size());
  const network::mojom::CookieDeletionFilterPtr& delete_filter =
      fake_cookie_manager()->delete_cookies_calls()[0];
  EXPECT_EQ(android_sms::GetAndroidMessagesURL(), delete_filter->url);
  EXPECT_EQ("default_to_persist", delete_filter->cookie_name);
}

TEST_F(AndroidSmsAppHelperDelegateImplTest, TestInstallAndLaunchMessagesApp) {
  // TODO(crbug/876972): Figure out how to actually test the launching of the
  // app here.
}

}  // namespace multidevice_setup

}  // namespace chromeos
