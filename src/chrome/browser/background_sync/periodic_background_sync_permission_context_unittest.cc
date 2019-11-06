// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_sync/periodic_background_sync_permission_context.h"

#include <string>

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestPeriodicBackgroundSyncPermissionContext
    : public PeriodicBackgroundSyncPermissionContext {
 public:
  explicit TestPeriodicBackgroundSyncPermissionContext(Profile* profile)
      : PeriodicBackgroundSyncPermissionContext(profile) {}

  void InstallPwa(const GURL& url) { installed_pwas_.insert(url); }

  // PeriodicBackgroundSyncPermissionContext overrides:
  bool IsPwaInstalled(const GURL& url) const override {
    return installed_pwas_.find(url) != installed_pwas_.end();
  }

 private:
  std::set<GURL> installed_pwas_;
};

class PeriodicBackgroundSyncPermissionContextTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  PeriodicBackgroundSyncPermissionContextTest() = default;
  ~PeriodicBackgroundSyncPermissionContextTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    permission_context_ =
        std::make_unique<TestPeriodicBackgroundSyncPermissionContext>(
            profile());
  }

  void TearDown() override {
    // The destructor for |permission_context_| needs a valid thread bundle.
    permission_context_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  ContentSetting GetPermissionStatus(const GURL& url, bool with_frame = false) {
    content::RenderFrameHost* render_frame_host = nullptr;

    if (with_frame) {
      content::WebContentsTester::For(web_contents())->NavigateAndCommit(url);
      render_frame_host = web_contents()->GetMainFrame();
    }

    auto permission_result = permission_context_->GetPermissionStatus(
        render_frame_host, /* requesting_origin= */ url,
        /* embedding_origin= */ url);
    return permission_result.content_setting;
  }

  void SetBackgroundSyncContentSetting(const GURL& url,
                                       ContentSetting setting) {
    auto* host_content_settings_map =
        HostContentSettingsMapFactory::GetForProfile(profile());
    ASSERT_TRUE(host_content_settings_map);
    host_content_settings_map->SetContentSettingDefaultScope(
        /* primary_url= */ url, /* secondary_url= */ url,
        CONTENT_SETTINGS_TYPE_BACKGROUND_SYNC,
        /* resource_identifier= */ std::string(), setting);
  }

  void InstallPwa(const GURL& url) { permission_context_->InstallPwa(url); }

  void SetUpPwaAndContentSettings(const GURL& url) {
    InstallPwa(url);
    SetBackgroundSyncContentSetting(url, CONTENT_SETTING_ALLOW);
  }

 private:
  std::unique_ptr<TestPeriodicBackgroundSyncPermissionContext>
      permission_context_;
  DISALLOW_COPY_AND_ASSIGN(PeriodicBackgroundSyncPermissionContextTest);
};

TEST_F(PeriodicBackgroundSyncPermissionContextTest, DenyWhenFeatureDisabled) {
  EXPECT_EQ(GetPermissionStatus(GURL("https://example.com")),
            CONTENT_SETTING_BLOCK);
}

TEST_F(PeriodicBackgroundSyncPermissionContextTest, DenyForInsecureOrigin) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kPeriodicBackgroundSync);
  GURL url("http://example.com");
  SetBackgroundSyncContentSetting(url, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(GetPermissionStatus(url, /* with_frame= */ false),
            CONTENT_SETTING_BLOCK);
}

TEST_F(PeriodicBackgroundSyncPermissionContextTest, AllowWithFrame) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kPeriodicBackgroundSync);
  GURL url("https://example.com");
  SetUpPwaAndContentSettings(url);
  SetBackgroundSyncContentSetting(url, CONTENT_SETTING_ALLOW);

  EXPECT_EQ(GetPermissionStatus(url, /* with_frame= */ true),
            CONTENT_SETTING_ALLOW);
}

TEST_F(PeriodicBackgroundSyncPermissionContextTest, AllowWithoutFrame) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kPeriodicBackgroundSync);
  GURL url("https://example.com");
  SetUpPwaAndContentSettings(url);

  EXPECT_EQ(GetPermissionStatus(url, /* with_frame= */ false),
            CONTENT_SETTING_ALLOW);
}

TEST_F(PeriodicBackgroundSyncPermissionContextTest, DesktopPWA) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kPeriodicBackgroundSync);
  GURL url("https://example.com");
  SetUpPwaAndContentSettings(url);

  EXPECT_EQ(GetPermissionStatus(url), CONTENT_SETTING_ALLOW);

  // Disable one-shot Background Sync.
  SetBackgroundSyncContentSetting(url, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(GetPermissionStatus(url), CONTENT_SETTING_BLOCK);
}

}  // namespace
