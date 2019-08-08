// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_launch_event_logger.h"

#include <memory>

#include "base/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "components/arc/arc_prefs.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/ukm/app_source_url_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_util.h"

namespace app_list {

const char kGmailChromeApp[] = "pjkljhegncpnkpknbcohdijeoejaedia";
const char kMapsArcApp[] = "gmhipfhgnoelkiiofcnimehjnpaejiel";
const char kPhotosPWAApp[] = "ncmjhecbjeaamljdfahankockkkdmedg";
const GURL kPhotosPWAUrl = GURL("http://photos.google.com/");

const char kMapsPackageName[] = "com.google.android.apps.maps";

namespace {

bool TestIsWebstoreExtension(base::StringPiece id) {
  return (id == kGmailChromeApp);
}

}  // namespace

class AppLaunchEventLoggerForTest : public AppLaunchEventLogger {
 protected:
  const GURL& GetLaunchWebURL(const extensions::Extension* extension) override {
    return kPhotosPWAUrl;
  }
};

class AppLaunchEventLoggerTest : public testing::Test {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(ukm::kUkmAppLogging);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;
};

TEST_F(AppLaunchEventLoggerTest, CheckUkmCodePWA) {
  extensions::ExtensionRegistry registry(nullptr);
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("test")
          .SetID(kPhotosPWAApp)
          .AddFlags(extensions::Extension::FROM_BOOKMARK)
          .Build();

  registry.AddEnabled(extension);

  AppLaunchEventLoggerForTest app_launch_event_logger_;
  app_launch_event_logger_.SetAppDataForTesting(&registry, nullptr, nullptr);
  app_launch_event_logger_.OnGridClicked(kPhotosPWAApp);

  scoped_task_environment_.RunUntilIdle();

  const auto entries = test_ukm_recorder_.GetEntriesByName("AppListAppLaunch");
  ASSERT_EQ(1ul, entries.size());
  const auto* entry = entries.back();
  test_ukm_recorder_.ExpectEntrySourceHasUrl(entry, kPhotosPWAUrl);
  test_ukm_recorder_.ExpectEntryMetric(entry, "AllClicksLast24Hours", 1);
  test_ukm_recorder_.ExpectEntryMetric(entry, "AllClicksLastHour", 1);
  test_ukm_recorder_.ExpectEntryMetric(entry, "AppType", 3);
  test_ukm_recorder_.ExpectEntryMetric(entry, "LaunchedFrom", 1);
  test_ukm_recorder_.ExpectEntryMetric(entry, "TotalHours", 0);

  const auto click_entries =
      test_ukm_recorder_.GetEntriesByName("AppListAppClickData");
  ASSERT_EQ(1ul, click_entries.size());
  const auto* photos_entry = click_entries.back();
  test_ukm_recorder_.ExpectEntrySourceHasUrl(photos_entry, kPhotosPWAUrl);
  test_ukm_recorder_.ExpectEntryMetric(photos_entry, "AppLaunchId",
                                       entry->source_id);
  test_ukm_recorder_.ExpectEntryMetric(photos_entry, "AppType", 3);
}

TEST_F(AppLaunchEventLoggerTest, CheckUkmCodeChrome) {
  extensions::ExtensionRegistry registry(nullptr);
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("test")
          .SetID(kGmailChromeApp)
          .AddFlags(extensions::Extension::FROM_WEBSTORE)
          .Build();
  registry.AddEnabled(extension);

  GURL url(std::string("chrome-extension://") + kGmailChromeApp + "/");

  url::AddStandardScheme("chrome-extension", url::SCHEME_WITH_HOST);
  test_ukm_recorder_.SetIsWebstoreExtensionCallback(
      base::BindRepeating(&TestIsWebstoreExtension));

  AppLaunchEventLoggerForTest app_launch_event_logger_;
  app_launch_event_logger_.SetAppDataForTesting(&registry, nullptr, nullptr);
  app_launch_event_logger_.OnGridClicked(kGmailChromeApp);

  scoped_task_environment_.RunUntilIdle();

  const auto entries = test_ukm_recorder_.GetEntriesByName("AppListAppLaunch");
  ASSERT_EQ(1ul, entries.size());
  const auto* entry = entries.back();
  test_ukm_recorder_.ExpectEntrySourceHasUrl(entry, url);
  test_ukm_recorder_.ExpectEntryMetric(entry, "AppType", 1);
  test_ukm_recorder_.ExpectEntryMetric(entry, "LaunchedFrom", 1);
}

TEST_F(AppLaunchEventLoggerTest, CheckUkmCodeArc) {
  base::Value package(base::Value::Type::DICTIONARY);
  package.SetKey(AppLaunchEventLogger::kShouldSync, base::Value(true));

  auto packages = std::make_unique<base::DictionaryValue>();
  packages->SetKey(kMapsPackageName, package.Clone());

  base::Value app(base::Value::Type::DICTIONARY);
  app.SetKey(AppLaunchEventLogger::kPackageName, base::Value(kMapsPackageName));

  auto arc_apps = std::make_unique<base::DictionaryValue>();
  arc_apps->SetKey(kMapsArcApp, app.Clone());

  GURL url("app://play/gbpfhehadcpcndihhameeacbdmbjbhgi");

  AppLaunchEventLoggerForTest app_launch_event_logger_;
  app_launch_event_logger_.SetAppDataForTesting(nullptr, arc_apps.get(),
                                                packages.get());
  app_launch_event_logger_.OnGridClicked(kMapsArcApp);

  scoped_task_environment_.RunUntilIdle();

  const auto entries = test_ukm_recorder_.GetEntriesByName("AppListAppLaunch");
  ASSERT_EQ(1ul, entries.size());
  const auto* entry = entries.back();
  test_ukm_recorder_.ExpectEntrySourceHasUrl(entry, url);
  test_ukm_recorder_.ExpectEntryMetric(entry, "AppType", 2);
  test_ukm_recorder_.ExpectEntryMetric(entry, "LaunchedFrom", 1);
}

TEST_F(AppLaunchEventLoggerTest, CheckMultipleClicks) {
  // Click on PWA photos, then Chrome Maps, then PWA Photos again.
  extensions::ExtensionRegistry registry(nullptr);
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("test")
          .SetID(kPhotosPWAApp)
          .AddFlags(extensions::Extension::FROM_BOOKMARK)
          .Build();
  registry.AddEnabled(extension);

  base::Value package(base::Value::Type::DICTIONARY);
  package.SetKey(AppLaunchEventLogger::kShouldSync, base::Value(true));

  auto packages = std::make_unique<base::DictionaryValue>();
  packages->SetKey(kMapsPackageName, package.Clone());

  base::Value app(base::Value::Type::DICTIONARY);
  app.SetKey(AppLaunchEventLogger::kPackageName, base::Value(kMapsPackageName));

  auto arc_apps = std::make_unique<base::DictionaryValue>();
  arc_apps->SetKey(kMapsArcApp, app.Clone());

  GURL maps_url("app://play/gbpfhehadcpcndihhameeacbdmbjbhgi");

  AppLaunchEventLoggerForTest app_launch_event_logger_;
  app_launch_event_logger_.SetAppDataForTesting(&registry, arc_apps.get(),
                                                packages.get());
  app_launch_event_logger_.OnGridClicked(kPhotosPWAApp);
  app_launch_event_logger_.OnGridClicked(kMapsArcApp);
  app_launch_event_logger_.OnSuggestionChipOrSearchBoxClicked(kPhotosPWAApp, 3,
                                                              2);
  app_launch_event_logger_.OnGridClicked(kPhotosPWAApp);

  scoped_task_environment_.RunUntilIdle();

  const auto entries = test_ukm_recorder_.GetEntriesByName("AppListAppLaunch");
  ASSERT_EQ(4ul, entries.size());
  const auto* entry = entries.back();
  test_ukm_recorder_.ExpectEntrySourceHasUrl(entry, kPhotosPWAUrl);
  test_ukm_recorder_.ExpectEntryMetric(entry, "AllClicksLast24Hours", 4);
  test_ukm_recorder_.ExpectEntryMetric(entry, "AllClicksLastHour", 4);
  test_ukm_recorder_.ExpectEntryMetric(entry, "AppType", 3);
  test_ukm_recorder_.ExpectEntryMetric(entry, "LaunchedFrom", 1);
  test_ukm_recorder_.ExpectEntryMetric(entry, "TotalHours", 0);

  const auto click_entries =
      test_ukm_recorder_.GetEntriesByName("AppListAppClickData");
  ASSERT_EQ(8ul, click_entries.size());
  // Examine the last two events, which are created by the last click.
  const auto* maps_entry = click_entries.at(6);
  const auto* photos_entry = click_entries.at(7);
  if (test_ukm_recorder_.GetSourceForSourceId(maps_entry->source_id)->url() ==
      kPhotosPWAUrl) {
    const auto* tmp_entry = photos_entry;
    photos_entry = maps_entry;
    maps_entry = tmp_entry;
  }
  test_ukm_recorder_.ExpectEntrySourceHasUrl(photos_entry, kPhotosPWAUrl);
  test_ukm_recorder_.ExpectEntrySourceHasUrl(maps_entry, maps_url);
  test_ukm_recorder_.ExpectEntryMetric(photos_entry, "AppLaunchId",
                                       entry->source_id);
  test_ukm_recorder_.ExpectEntryMetric(maps_entry, "AppLaunchId",
                                       entry->source_id);
  test_ukm_recorder_.ExpectEntryMetric(photos_entry, "ClicksLast24Hours", 2);
  test_ukm_recorder_.ExpectEntryMetric(maps_entry, "ClicksLast24Hours", 1);
  test_ukm_recorder_.ExpectEntryMetric(photos_entry, "ClicksLastHour", 2);
  test_ukm_recorder_.ExpectEntryMetric(maps_entry, "ClicksLastHour", 1);
  test_ukm_recorder_.ExpectEntryMetric(photos_entry, "MostRecentlyUsedIndex",
                                       0);
  test_ukm_recorder_.ExpectEntryMetric(maps_entry, "MostRecentlyUsedIndex", 1);
  test_ukm_recorder_.ExpectEntryMetric(photos_entry, "TimeSinceLastClick", 0);
  test_ukm_recorder_.ExpectEntryMetric(photos_entry, "TotalClicks", 2);
  test_ukm_recorder_.ExpectEntryMetric(maps_entry, "TotalClicks", 1);
  test_ukm_recorder_.ExpectEntryMetric(photos_entry, "LastLaunchedFrom", 2);
  test_ukm_recorder_.ExpectEntryMetric(maps_entry, "LastLaunchedFrom", 1);
}

TEST_F(AppLaunchEventLoggerTest, CheckUkmCodeSuggestionChip) {
  extensions::ExtensionRegistry registry(nullptr);
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("test")
          .SetID(kPhotosPWAApp)
          .AddFlags(extensions::Extension::FROM_BOOKMARK)
          .Build();
  registry.AddEnabled(extension);

  AppLaunchEventLoggerForTest app_launch_event_logger_;
  app_launch_event_logger_.SetAppDataForTesting(&registry, nullptr, nullptr);
  app_launch_event_logger_.OnSuggestionChipOrSearchBoxClicked(kPhotosPWAApp, 3,
                                                              2);

  scoped_task_environment_.RunUntilIdle();

  const auto entries = test_ukm_recorder_.GetEntriesByName("AppListAppLaunch");
  ASSERT_EQ(1ul, entries.size());
  const auto* entry = entries.back();
  test_ukm_recorder_.ExpectEntrySourceHasUrl(entry, kPhotosPWAUrl);
  test_ukm_recorder_.ExpectEntryMetric(entry, "PositionIndex", 3);
  test_ukm_recorder_.ExpectEntryMetric(entry, "LaunchedFrom", 2);
}

TEST_F(AppLaunchEventLoggerTest, CheckUkmCodeSearchBox) {
  extensions::ExtensionRegistry registry(nullptr);
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("test")
          .SetID(kPhotosPWAApp)
          .AddFlags(extensions::Extension::FROM_BOOKMARK)
          .Build();
  registry.AddEnabled(extension);

  AppLaunchEventLoggerForTest app_launch_event_logger_;
  app_launch_event_logger_.SetAppDataForTesting(&registry, nullptr, nullptr);
  app_launch_event_logger_.OnSuggestionChipOrSearchBoxClicked(kPhotosPWAApp, 3,
                                                              4);

  scoped_task_environment_.RunUntilIdle();

  const auto entries = test_ukm_recorder_.GetEntriesByName("AppListAppLaunch");
  ASSERT_EQ(1ul, entries.size());
  const auto* entry = entries.back();
  test_ukm_recorder_.ExpectEntrySourceHasUrl(entry, kPhotosPWAUrl);
  test_ukm_recorder_.ExpectEntryMetric(entry, "PositionIndex", 3);
  test_ukm_recorder_.ExpectEntryMetric(entry, "LaunchedFrom", 4);
}

}  // namespace app_list
