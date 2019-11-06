// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/apps_navigation_throttle.h"
#include "chrome/browser/apps/intent_helper/apps_navigation_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace apps {

TEST(AppsNavigationThrottleTest, TestIsGoogleRedirectorUrl) {
  // Test that redirect urls with different TLDs are still recognized.
  EXPECT_TRUE(AppsNavigationThrottle::IsGoogleRedirectorUrlForTesting(
      GURL("https://www.google.com.au/url?q=wathever")));
  EXPECT_TRUE(AppsNavigationThrottle::IsGoogleRedirectorUrlForTesting(
      GURL("https://www.google.com.mx/url?q=hotpot")));
  EXPECT_TRUE(AppsNavigationThrottle::IsGoogleRedirectorUrlForTesting(
      GURL("https://www.google.co/url?q=query")));

  // Non-google domains shouldn't be used as valid redirect links.
  EXPECT_FALSE(AppsNavigationThrottle::IsGoogleRedirectorUrlForTesting(
      GURL("https://www.not-google.com/url?q=query")));
  EXPECT_FALSE(AppsNavigationThrottle::IsGoogleRedirectorUrlForTesting(
      GURL("https://www.gooogle.com/url?q=legit_query")));

  // This method only takes "/url" as a valid path, it needs to contain a query,
  // we don't analyze that query as it will expand later on in the same
  // throttle.
  EXPECT_TRUE(AppsNavigationThrottle::IsGoogleRedirectorUrlForTesting(
      GURL("https://www.google.com/url?q=who_dis")));
  EXPECT_TRUE(AppsNavigationThrottle::IsGoogleRedirectorUrlForTesting(
      GURL("http://www.google.com/url?q=who_dis")));
  EXPECT_FALSE(AppsNavigationThrottle::IsGoogleRedirectorUrlForTesting(
      GURL("https://www.google.com/url")));
  EXPECT_FALSE(AppsNavigationThrottle::IsGoogleRedirectorUrlForTesting(
      GURL("https://www.google.com/link?q=query")));
  EXPECT_FALSE(AppsNavigationThrottle::IsGoogleRedirectorUrlForTesting(
      GURL("https://www.google.com/link")));
}

TEST(AppsNavigationThrottleTest, TestShouldOverrideUrlLoading) {
  // If either of two paramters is empty, the function should return false.
  EXPECT_FALSE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL(), GURL("http://a.google.com/")));
  EXPECT_FALSE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://a.google.com/"), GURL()));
  EXPECT_FALSE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL(), GURL()));

  // A navigation to an a url that is neither an http nor https scheme cannot be
  // override.
  EXPECT_FALSE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://www.a.com"), GURL("chrome-extension://fake_document")));
  EXPECT_FALSE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("https://www.a.com"), GURL("chrome-extension://fake_document")));
  EXPECT_FALSE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://www.a.com"), GURL("chrome://fake_document")));
  EXPECT_FALSE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://www.a.com"), GURL("file://fake_document")));
  EXPECT_FALSE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("https://www.a.com"), GURL("chrome://fake_document")));
  EXPECT_FALSE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("https://www.a.com"), GURL("file://fake_document")));

  // A navigation from chrome-extension scheme cannot be overriden.
  EXPECT_FALSE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("chrome-extension://fake_document"), GURL("http://www.a.com")));
  EXPECT_FALSE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("chrome-extension://fake_document"), GURL("https://www.a.com")));
  EXPECT_FALSE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("chrome-extension://fake_a"), GURL("chrome-extension://fake_b")));

  // Other navigations can be overridden.
  EXPECT_TRUE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://www.google.com"), GURL("http://www.not-google.com/")));
  EXPECT_TRUE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://www.not-google.com"), GURL("http://www.google.com/")));
  EXPECT_TRUE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://www.google.com"), GURL("http://www.google.com/")));
  EXPECT_TRUE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://a.google.com"), GURL("http://b.google.com/")));
  EXPECT_TRUE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://a.not-google.com"), GURL("http://b.not-google.com")));
  EXPECT_TRUE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("chrome://fake_document"), GURL("http://www.a.com")));
  EXPECT_TRUE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("file://fake_document"), GURL("http://www.a.com")));
  EXPECT_TRUE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("chrome://fake_document"), GURL("https://www.a.com")));
  EXPECT_TRUE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("file://fake_document"), GURL("https://www.a.com")));

  // A navigation going to a redirect url cannot be overridden, unless there's
  // no query or the path is not valid.
  EXPECT_FALSE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://www.google.com"), GURL("https://www.google.com/url?q=b")));
  EXPECT_FALSE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("https://www.a.com"), GURL("https://www.google.com/url?q=a")));
  EXPECT_TRUE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("https://www.a.com"), GURL("https://www.google.com/url")));
  EXPECT_TRUE(AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("https://www.a.com"), GURL("https://www.google.com/link?q=a")));
}

TEST(AppsNavigationThrottleTest, TestGetPickerAction) {
  // Expect PickerAction::PICKER_ERROR if the close_reason is ERROR.
  EXPECT_EQ(
      AppsNavigationThrottle::PickerAction::PICKER_ERROR,
      AppsNavigationThrottle::GetPickerAction(
          apps::mojom::AppType::kUnknown, IntentPickerCloseReason::PICKER_ERROR,
          /*should_persist=*/true));

  EXPECT_EQ(
      AppsNavigationThrottle::PickerAction::PICKER_ERROR,
      AppsNavigationThrottle::GetPickerAction(
          apps::mojom::AppType::kArc, IntentPickerCloseReason::PICKER_ERROR,
          /*should_persist=*/true));

  EXPECT_EQ(
      AppsNavigationThrottle::PickerAction::PICKER_ERROR,
      AppsNavigationThrottle::GetPickerAction(
          apps::mojom::AppType::kUnknown, IntentPickerCloseReason::PICKER_ERROR,
          /*should_persist=*/false));

  EXPECT_EQ(
      AppsNavigationThrottle::PickerAction::PICKER_ERROR,
      AppsNavigationThrottle::GetPickerAction(
          apps::mojom::AppType::kArc, IntentPickerCloseReason::PICKER_ERROR,
          /*should_persist=*/false));

  // Expect PickerAction::DIALOG_DEACTIVATED if the close_reason is
  // DIALOG_DEACTIVATED.
  EXPECT_EQ(AppsNavigationThrottle::PickerAction::DIALOG_DEACTIVATED,
            AppsNavigationThrottle::GetPickerAction(
                apps::mojom::AppType::kUnknown,
                IntentPickerCloseReason::DIALOG_DEACTIVATED,
                /*should_persist=*/true));

  EXPECT_EQ(AppsNavigationThrottle::PickerAction::DIALOG_DEACTIVATED,
            AppsNavigationThrottle::GetPickerAction(
                apps::mojom::AppType::kArc,
                IntentPickerCloseReason::DIALOG_DEACTIVATED,
                /*should_persist=*/true));

  EXPECT_EQ(AppsNavigationThrottle::PickerAction::DIALOG_DEACTIVATED,
            AppsNavigationThrottle::GetPickerAction(
                apps::mojom::AppType::kUnknown,
                IntentPickerCloseReason::DIALOG_DEACTIVATED,
                /*should_persist=*/false));

  EXPECT_EQ(AppsNavigationThrottle::PickerAction::DIALOG_DEACTIVATED,
            AppsNavigationThrottle::GetPickerAction(
                apps::mojom::AppType::kArc,
                IntentPickerCloseReason::DIALOG_DEACTIVATED,
                /*should_persist=*/false));

  // Expect PickerAction::PREFERRED_ACTIVITY_FOUND if the close_reason is
  // PREFERRED_APP_FOUND.
  EXPECT_EQ(AppsNavigationThrottle::PickerAction::PREFERRED_ACTIVITY_FOUND,
            AppsNavigationThrottle::GetPickerAction(
                apps::mojom::AppType::kUnknown,
                IntentPickerCloseReason::PREFERRED_APP_FOUND,
                /*should_persist=*/true));

  EXPECT_EQ(AppsNavigationThrottle::PickerAction::PREFERRED_ACTIVITY_FOUND,
            AppsNavigationThrottle::GetPickerAction(
                apps::mojom::AppType::kArc,
                IntentPickerCloseReason::PREFERRED_APP_FOUND,
                /*should_persist=*/true));

  EXPECT_EQ(AppsNavigationThrottle::PickerAction::PREFERRED_ACTIVITY_FOUND,
            AppsNavigationThrottle::GetPickerAction(
                apps::mojom::AppType::kUnknown,
                IntentPickerCloseReason::PREFERRED_APP_FOUND,
                /*should_persist=*/false));

  EXPECT_EQ(AppsNavigationThrottle::PickerAction::PREFERRED_ACTIVITY_FOUND,
            AppsNavigationThrottle::GetPickerAction(
                apps::mojom::AppType::kArc,
                IntentPickerCloseReason::PREFERRED_APP_FOUND,
                /*should_persist=*/false));

  // Expect PREFERRED depending on the value of |should_persist|, and |app_type|
  // to be ignored if reason is STAY_IN_CHROME.
  EXPECT_EQ(AppsNavigationThrottle::PickerAction::CHROME_PREFERRED_PRESSED,
            AppsNavigationThrottle::GetPickerAction(
                apps::mojom::AppType::kUnknown,
                IntentPickerCloseReason::STAY_IN_CHROME,
                /*should_persist=*/true));

  EXPECT_EQ(
      AppsNavigationThrottle::PickerAction::CHROME_PREFERRED_PRESSED,
      AppsNavigationThrottle::GetPickerAction(
          apps::mojom::AppType::kArc, IntentPickerCloseReason::STAY_IN_CHROME,
          /*should_persist=*/true));

  EXPECT_EQ(AppsNavigationThrottle::PickerAction::CHROME_PRESSED,
            AppsNavigationThrottle::GetPickerAction(
                apps::mojom::AppType::kUnknown,
                IntentPickerCloseReason::STAY_IN_CHROME,
                /*should_persist=*/false));

  EXPECT_EQ(
      AppsNavigationThrottle::PickerAction::CHROME_PRESSED,
      AppsNavigationThrottle::GetPickerAction(
          apps::mojom::AppType::kArc, IntentPickerCloseReason::STAY_IN_CHROME,
          /*should_persist=*/false));

  // Expect PREFERRED depending on the value of |should_persist|, and
  // INVALID/ARC to be chosen if reason is OPEN_APP.
  EXPECT_EQ(
      AppsNavigationThrottle::PickerAction::INVALID,
      AppsNavigationThrottle::GetPickerAction(apps::mojom::AppType::kUnknown,
                                              IntentPickerCloseReason::OPEN_APP,
                                              /*should_persist=*/true));

  EXPECT_EQ(AppsNavigationThrottle::PickerAction::ARC_APP_PREFERRED_PRESSED,
            AppsNavigationThrottle::GetPickerAction(
                apps::mojom::AppType::kArc, IntentPickerCloseReason::OPEN_APP,
                /*should_persist=*/true));

  EXPECT_EQ(
      AppsNavigationThrottle::PickerAction::INVALID,
      AppsNavigationThrottle::GetPickerAction(apps::mojom::AppType::kUnknown,
                                              IntentPickerCloseReason::OPEN_APP,
                                              /*should_persist=*/false));

  EXPECT_EQ(AppsNavigationThrottle::PickerAction::ARC_APP_PRESSED,
            AppsNavigationThrottle::GetPickerAction(
                apps::mojom::AppType::kArc, IntentPickerCloseReason::OPEN_APP,
                /*should_persist=*/false));
}

TEST(AppsNavigationThrottleTest, TestGetDestinationPlatform) {
  const std::string app_id = "fake_package";

  // When the PickerAction is either ERROR or DIALOG_DEACTIVATED we MUST stay in
  // Chrome not taking into account the selected_app_package.
  EXPECT_EQ(AppsNavigationThrottle::Platform::CHROME,
            AppsNavigationThrottle::GetDestinationPlatform(
                app_id, AppsNavigationThrottle::PickerAction::PICKER_ERROR));
  EXPECT_EQ(AppsNavigationThrottle::Platform::CHROME,
            AppsNavigationThrottle::GetDestinationPlatform(
                app_id, AppsNavigationThrottle::PickerAction::PICKER_ERROR));
  EXPECT_EQ(
      AppsNavigationThrottle::Platform::CHROME,
      AppsNavigationThrottle::GetDestinationPlatform(
          app_id, AppsNavigationThrottle::PickerAction::DIALOG_DEACTIVATED));
  EXPECT_EQ(
      AppsNavigationThrottle::Platform::CHROME,
      AppsNavigationThrottle::GetDestinationPlatform(
          app_id, AppsNavigationThrottle::PickerAction::DIALOG_DEACTIVATED));

  // When the PickerAction is PWA_APP_PRESSED, always expect the platform to be
  // PWA.
  EXPECT_EQ(AppsNavigationThrottle::Platform::PWA,
            AppsNavigationThrottle::GetDestinationPlatform(
                app_id, AppsNavigationThrottle::PickerAction::PWA_APP_PRESSED));

  EXPECT_EQ(AppsNavigationThrottle::Platform::PWA,
            AppsNavigationThrottle::GetDestinationPlatform(
                app_id, AppsNavigationThrottle::PickerAction::PWA_APP_PRESSED));

  // TODO(crbug.com/939205): restore testing ARC picker redirection
}

}  // namespace apps
