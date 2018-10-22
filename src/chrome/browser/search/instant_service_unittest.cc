// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/instant_service.h"

#include <vector>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/search/instant_unittest_base.h"
#include "chrome/browser/search/ntp_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/search/instant_types.h"
#include "chrome/common/url_constants.h"
#include "components/ntp_tiles/constants.h"
#include "components/ntp_tiles/ntp_tile.h"
#include "components/ntp_tiles/section_type.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using InstantServiceTest = InstantUnitTestBase;

class InstantServiceTestCustomLinksEnabled : public InstantServiceTest {
 public:
  InstantServiceTestCustomLinksEnabled() {
    scoped_feature_list_.InitAndEnableFeature(ntp_tiles::kNtpCustomLinks);
  }
  ~InstantServiceTestCustomLinksEnabled() override {}

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(InstantServiceTestCustomLinksEnabled);
};

class InstantServiceTestCustomBackgroundsEnabled : public InstantServiceTest {
 public:
  InstantServiceTestCustomBackgroundsEnabled() {
    scoped_feature_list_.InitAndEnableFeature(features::kNtpBackgrounds);
  }
  ~InstantServiceTestCustomBackgroundsEnabled() override {}

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(InstantServiceTestCustomBackgroundsEnabled);
};

TEST_F(InstantServiceTest, GetNTPTileSuggestion) {
  ntp_tiles::NTPTile some_tile;
  some_tile.source = ntp_tiles::TileSource::TOP_SITES;
  some_tile.title_source = ntp_tiles::TileTitleSource::TITLE_TAG;
  ntp_tiles::NTPTilesVector suggestions{some_tile};

  std::map<ntp_tiles::SectionType, ntp_tiles::NTPTilesVector> suggestions_map;
  suggestions_map[ntp_tiles::SectionType::PERSONALIZED] = suggestions;

  instant_service_->OnURLsAvailable(suggestions_map);

  auto items = instant_service_->most_visited_items_;
  ASSERT_EQ(1, (int)items.size());
  EXPECT_EQ(ntp_tiles::TileSource::TOP_SITES, items[0].source);
  EXPECT_EQ(ntp_tiles::TileTitleSource::TITLE_TAG, items[0].title_source);
}

TEST_F(InstantServiceTestCustomLinksEnabled,
       DisableUndoCustomLinkActionForNonGoogleSearchProvider) {
  SetUserSelectedDefaultSearchProvider("{google:baseURL}");
  EXPECT_TRUE(instant_service_->UndoCustomLinkAction());

  SetUserSelectedDefaultSearchProvider("https://www.search.com");
  EXPECT_FALSE(instant_service_->UndoCustomLinkAction());
}

TEST_F(InstantServiceTestCustomLinksEnabled,
       DisableResetCustomLinksForNonGoogleSearchProvider) {
  SetUserSelectedDefaultSearchProvider("{google:baseURL}");
  EXPECT_TRUE(instant_service_->ResetCustomLinks());

  SetUserSelectedDefaultSearchProvider("https://www.search.com");
  EXPECT_FALSE(instant_service_->ResetCustomLinks());
}

TEST_F(InstantServiceTestCustomBackgroundsEnabled, SetCustomBackgroundURL) {
  const GURL kUrl("https://www.foo.com");
  instant_service_->SetCustomBackgroundURL(kUrl);

  ThemeBackgroundInfo* theme_info = instant_service_->GetThemeInfoForTesting();
  EXPECT_EQ(kUrl, theme_info->custom_background_url);
}

TEST_F(InstantServiceTest, SetCustomBackgroundURL) {
  const GURL kUrl("https://www.foo.com");
  instant_service_->SetCustomBackgroundURL(kUrl);

  ThemeBackgroundInfo* theme_info = instant_service_->GetThemeInfoForTesting();
  EXPECT_EQ(GURL(), theme_info->custom_background_url);
}

TEST_F(InstantServiceTestCustomBackgroundsEnabled,
       SetCustomBackgroundURLInvalidURL) {
  const GURL kUrl("foo");
  instant_service_->SetCustomBackgroundURL(kUrl);

  ThemeBackgroundInfo* theme_info = instant_service_->GetThemeInfoForTesting();
  EXPECT_EQ(std::string(), theme_info->custom_background_url.spec());
}

TEST_F(InstantServiceTestCustomBackgroundsEnabled,
       SetCustomBackgroundURLWithAttributions) {
  const GURL kUrl("https://www.foo.com");
  const std::string kAttributionLine1 = "foo";
  const std::string kAttributionLine2 = "bar";
  const GURL kActionUrl("https://www.bar.com");
  instant_service_->SetCustomBackgroundURLWithAttributions(
      kUrl, kAttributionLine1, kAttributionLine2, kActionUrl);

  ThemeBackgroundInfo* theme_info = instant_service_->GetThemeInfoForTesting();
  EXPECT_EQ(kUrl, theme_info->custom_background_url);
  EXPECT_EQ(kAttributionLine1,
            theme_info->custom_background_attribution_line_1);
  EXPECT_EQ(kAttributionLine2,
            theme_info->custom_background_attribution_line_2);
  EXPECT_EQ(kActionUrl, theme_info->custom_background_attribution_action_url);
}

TEST_F(InstantServiceTestCustomBackgroundsEnabled,
       ChangingSearchProviderClearsThemeInfoAndPref) {
  const GURL kUrl("https://www.foo.com");
  const std::string kAttributionLine1 = "foo";
  const std::string kAttributionLine2 = "bar";
  const GURL kActionUrl("https://www.bar.com");

  SetUserSelectedDefaultSearchProvider("{google:baseURL}");
  instant_service_->SetCustomBackgroundURLWithAttributions(
      kUrl, kAttributionLine1, kAttributionLine2, kActionUrl);

  ThemeBackgroundInfo* theme_info = instant_service_->GetThemeInfoForTesting();
  EXPECT_EQ(kUrl, theme_info->custom_background_url);
  EXPECT_EQ(kAttributionLine1,
            theme_info->custom_background_attribution_line_1);
  EXPECT_EQ(kAttributionLine2,
            theme_info->custom_background_attribution_line_2);
  EXPECT_EQ(kActionUrl, theme_info->custom_background_attribution_action_url);

  SetUserSelectedDefaultSearchProvider("https://www.search.com");
  instant_service_->UpdateThemeInfo();

  theme_info = instant_service_->GetThemeInfoForTesting();
  EXPECT_EQ(GURL(), theme_info->custom_background_url);
  EXPECT_EQ(std::string(), theme_info->custom_background_attribution_line_1);
  EXPECT_EQ(std::string(), theme_info->custom_background_attribution_line_2);
  EXPECT_EQ(GURL(), theme_info->custom_background_attribution_action_url);

  SetUserSelectedDefaultSearchProvider("{google:baseURL}");
  instant_service_->UpdateThemeInfo();

  theme_info = instant_service_->GetThemeInfoForTesting();
  EXPECT_EQ(GURL(), theme_info->custom_background_url);
  EXPECT_EQ(std::string(), theme_info->custom_background_attribution_line_1);
  EXPECT_EQ(std::string(), theme_info->custom_background_attribution_line_2);
  EXPECT_EQ(GURL(), theme_info->custom_background_attribution_action_url);
}

TEST_F(InstantServiceTestCustomBackgroundsEnabled,
       ChangingSearchProviderRemovesLocalBackgroundImageCopy) {
  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  base::FilePath path(user_data_dir.AppendASCII(
      chrome::kChromeSearchLocalNtpBackgroundFilename));

  base::WriteFile(path, "background_image", 16);

  SetUserSelectedDefaultSearchProvider("https://www.search.com");
  instant_service_->UpdateThemeInfo();

  base::TaskScheduler::GetInstance()->FlushForTesting();

  bool file_exists = base::PathExists(path);

  EXPECT_EQ(false, file_exists);
}

TEST_F(InstantServiceTestCustomBackgroundsEnabled,
       CustomBackgroundAttributionActionUrlReset) {
  const GURL kUrl("https://www.foo.com");
  const std::string kAttributionLine1 = "foo";
  const std::string kAttributionLine2 = "bar";
  const GURL kHttpsActionUrl("https://www.bar.com");
  const GURL kHttpActionUrl("http://www.bar.com");

  SetUserSelectedDefaultSearchProvider("{google:baseURL}");
  instant_service_->SetCustomBackgroundURLWithAttributions(
      kUrl, kAttributionLine1, kAttributionLine2, kHttpsActionUrl);

  ThemeBackgroundInfo* theme_info = instant_service_->GetThemeInfoForTesting();
  EXPECT_EQ(kHttpsActionUrl,
            theme_info->custom_background_attribution_action_url);

  instant_service_->SetCustomBackgroundURLWithAttributions(
      kUrl, kAttributionLine1, kAttributionLine2, kHttpActionUrl);

  theme_info = instant_service_->GetThemeInfoForTesting();
  EXPECT_EQ(GURL(), theme_info->custom_background_attribution_action_url);

  instant_service_->SetCustomBackgroundURLWithAttributions(
      kUrl, kAttributionLine1, kAttributionLine2, kHttpsActionUrl);

  theme_info = instant_service_->GetThemeInfoForTesting();
  EXPECT_EQ(kHttpsActionUrl,
            theme_info->custom_background_attribution_action_url);

  instant_service_->SetCustomBackgroundURLWithAttributions(
      kUrl, kAttributionLine1, kAttributionLine2, GURL());

  theme_info = instant_service_->GetThemeInfoForTesting();
  EXPECT_EQ(GURL(), theme_info->custom_background_attribution_action_url);
}
