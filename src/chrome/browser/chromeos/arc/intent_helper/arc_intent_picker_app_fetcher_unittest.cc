// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/chromeos/arc/intent_helper/arc_intent_picker_app_fetcher.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

// Creates an array with |num_elements| handlers and makes |chrome_index|-th
// handler "Chrome". If Chrome is not necessary, set |chrome_index| to
// |num_elements|.
std::vector<mojom::IntentHandlerInfoPtr> CreateArray(size_t num_elements,
                                                     size_t chrome_index) {
  std::vector<mojom::IntentHandlerInfoPtr> handlers;
  for (size_t i = 0; i < num_elements; ++i) {
    mojom::IntentHandlerInfoPtr handler = mojom::IntentHandlerInfo::New();
    handler->name = "Name";
    if (i == chrome_index) {
      handler->package_name =
          ArcIntentHelperBridge::kArcIntentHelperPackageName;
    } else {
      handler->package_name = "com.package";
    }
    handlers.push_back(std::move(handler));
  }
  return handlers;
}

}  // namespace

TEST(ArcIntentPickerAppFetcherTest, TestIsAppAvailable) {
  // Test an empty array.
  EXPECT_FALSE(
      ArcIntentPickerAppFetcher::IsAppAvailableForTesting(CreateArray(0, 0)));
  // Chrome only.
  EXPECT_FALSE(
      ArcIntentPickerAppFetcher::IsAppAvailableForTesting(CreateArray(1, 0)));
  // Chrome and another app.
  EXPECT_TRUE(
      ArcIntentPickerAppFetcher::IsAppAvailableForTesting(CreateArray(2, 0)));
  EXPECT_TRUE(
      ArcIntentPickerAppFetcher::IsAppAvailableForTesting(CreateArray(2, 1)));
  // App(s) only. This doesn't happen on production though.
  EXPECT_TRUE(
      ArcIntentPickerAppFetcher::IsAppAvailableForTesting(CreateArray(1, 1)));
  EXPECT_TRUE(
      ArcIntentPickerAppFetcher::IsAppAvailableForTesting(CreateArray(2, 2)));
}

TEST(ArcIntentPickerAppFetcherTest, TestFindPreferredApp) {
  // Test an empty array.
  EXPECT_EQ(0u, ArcIntentPickerAppFetcher::FindPreferredAppForTesting(
                    CreateArray(0, 0)));
  // Test no-preferred-app cases.
  EXPECT_EQ(1u, ArcIntentPickerAppFetcher::FindPreferredAppForTesting(
                    CreateArray(1, 0)));
  EXPECT_EQ(2u, ArcIntentPickerAppFetcher::FindPreferredAppForTesting(
                    CreateArray(2, 1)));
  EXPECT_EQ(3u, ArcIntentPickerAppFetcher::FindPreferredAppForTesting(
                    CreateArray(3, 2)));
  // Add a preferred app and call the function.
  for (size_t i = 0; i < 3; ++i) {
    std::vector<mojom::IntentHandlerInfoPtr> handlers = CreateArray(3, 0);
    handlers[i]->is_preferred = true;
    EXPECT_EQ(i,
              ArcIntentPickerAppFetcher::FindPreferredAppForTesting(handlers))
        << i;
  }
}

TEST(ArcIntentPickerAppFetcherTest, TestGetAppIndex) {
  const std::string package_name =
      ArcIntentHelperBridge::kArcIntentHelperPackageName;
  // Test an empty array.
  EXPECT_EQ(0u, ArcIntentPickerAppFetcher::GetAppIndex(CreateArray(0, 0),
                                                       package_name));
  // Test Chrome-only case.
  EXPECT_EQ(0u, ArcIntentPickerAppFetcher::GetAppIndex(CreateArray(1, 0),
                                                       package_name));
  // Test not-found cases.
  EXPECT_EQ(1u, ArcIntentPickerAppFetcher::GetAppIndex(CreateArray(1, 1),
                                                       package_name));
  EXPECT_EQ(2u, ArcIntentPickerAppFetcher::GetAppIndex(CreateArray(2, 2),
                                                       package_name));
  // Test other cases.
  EXPECT_EQ(0u, ArcIntentPickerAppFetcher::GetAppIndex(CreateArray(2, 0),
                                                       package_name));
  EXPECT_EQ(1u, ArcIntentPickerAppFetcher::GetAppIndex(CreateArray(2, 1),
                                                       package_name));
  EXPECT_EQ(0u, ArcIntentPickerAppFetcher::GetAppIndex(CreateArray(3, 0),
                                                       package_name));
  EXPECT_EQ(1u, ArcIntentPickerAppFetcher::GetAppIndex(CreateArray(3, 1),
                                                       package_name));
  EXPECT_EQ(2u, ArcIntentPickerAppFetcher::GetAppIndex(CreateArray(3, 2),
                                                       package_name));
}

}  // namespace arc
