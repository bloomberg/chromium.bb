// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class PermissionManagerTestingProfile : public TestingProfile {
 public:
  PermissionManagerTestingProfile() = default;
  ~PermissionManagerTestingProfile() override = default;
  PermissionManagerTestingProfile(const PermissionManagerTestingProfile&) =
      delete;
  PermissionManagerTestingProfile& operator=(
      const PermissionManagerTestingProfile&) = delete;

  permissions::PermissionManager* GetPermissionControllerDelegate() override {
    return PermissionManagerFactory::GetForProfile(this);
  }
};
}  // namespace

class ChromePermissionManagerTest : public ChromeRenderViewHostTestHarness {
 protected:
  permissions::PermissionManager* GetPermissionControllerDelegate() {
    return profile_->GetPermissionControllerDelegate();
  }

 private:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    profile_ = std::make_unique<PermissionManagerTestingProfile>();
  }

  void TearDown() override {
    profile_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<PermissionManagerTestingProfile> profile_;
};

TEST_F(ChromePermissionManagerTest, GetCanonicalOriginSearch) {
  const GURL google_com("https://www.google.com");
  const GURL google_de("https://www.google.de");
  const GURL other_url("https://other.url");
  const GURL google_base =
      GURL(UIThreadSearchTermsData().GoogleBaseURLValue()).GetOrigin();
  const GURL local_ntp = GURL(chrome::kChromeSearchLocalNtpUrl).GetOrigin();
  const GURL remote_ntp = GURL(std::string("chrome-search://") +
                               chrome::kChromeSearchRemoteNtpHost);
  const GURL other_chrome_search = GURL("chrome-search://not-local-ntp");
  const GURL top_level_ntp(chrome::kChromeUINewTabURL);

  // "Normal" URLs are not affected by GetCanonicalOrigin.
  EXPECT_EQ(google_com,
            GetPermissionControllerDelegate()->GetCanonicalOrigin(
                ContentSettingsType::GEOLOCATION, google_com, google_com));
  EXPECT_EQ(google_de,
            GetPermissionControllerDelegate()->GetCanonicalOrigin(
                ContentSettingsType::GEOLOCATION, google_de, google_de));
  EXPECT_EQ(other_url,
            GetPermissionControllerDelegate()->GetCanonicalOrigin(
                ContentSettingsType::GEOLOCATION, other_url, other_url));
  EXPECT_EQ(google_base,
            GetPermissionControllerDelegate()->GetCanonicalOrigin(
                ContentSettingsType::GEOLOCATION, google_base, google_base));

  // The local NTP URL gets mapped to the Google base URL.
  EXPECT_EQ(google_base,
            GetPermissionControllerDelegate()->GetCanonicalOrigin(
                ContentSettingsType::GEOLOCATION, local_ntp, top_level_ntp));
  // However, other chrome-search:// URLs, including the remote NTP URL, are
  // not affected.
  EXPECT_EQ(remote_ntp,
            GetPermissionControllerDelegate()->GetCanonicalOrigin(
                ContentSettingsType::GEOLOCATION, remote_ntp, top_level_ntp));
  EXPECT_EQ(google_com,
            GetPermissionControllerDelegate()->GetCanonicalOrigin(
                ContentSettingsType::GEOLOCATION, google_com, top_level_ntp));
  EXPECT_EQ(other_chrome_search,
            GetPermissionControllerDelegate()->GetCanonicalOrigin(
                ContentSettingsType::GEOLOCATION, other_chrome_search,
                top_level_ntp));
}

TEST_F(ChromePermissionManagerTest, GetCanonicalOriginPermissionDelegation) {
  const GURL requesting_origin("https://www.requesting.com");
  const GURL embedding_origin("https://www.google.de");
  const GURL extensions_requesting_origin(
      "chrome-extension://abcdefghijklmnopqrstuvxyz");

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndDisableFeature(
        permissions::features::kPermissionDelegation);
    // Without permission delegation enabled the requesting origin should always
    // be returned.
    EXPECT_EQ(requesting_origin,
              GetPermissionControllerDelegate()->GetCanonicalOrigin(
                  ContentSettingsType::GEOLOCATION, requesting_origin,
                  embedding_origin));
    EXPECT_EQ(extensions_requesting_origin,
              GetPermissionControllerDelegate()->GetCanonicalOrigin(
                  ContentSettingsType::GEOLOCATION,
                  extensions_requesting_origin, embedding_origin));
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeature(
        permissions::features::kPermissionDelegation);
    // With permission delegation, the embedding origin should be returned
    // except in the case of extensions; and except for notifications, for which
    // permission delegation is always off.
    EXPECT_EQ(embedding_origin,
              GetPermissionControllerDelegate()->GetCanonicalOrigin(
                  ContentSettingsType::GEOLOCATION, requesting_origin,
                  embedding_origin));
    EXPECT_EQ(extensions_requesting_origin,
              GetPermissionControllerDelegate()->GetCanonicalOrigin(
                  ContentSettingsType::GEOLOCATION,
                  extensions_requesting_origin, embedding_origin));
    EXPECT_EQ(requesting_origin,
              GetPermissionControllerDelegate()->GetCanonicalOrigin(
                  ContentSettingsType::NOTIFICATIONS, requesting_origin,
                  embedding_origin));
  }
}
