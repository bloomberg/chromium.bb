// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/web_app_icon_waiter.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "skia/ext/skia_utils_base.h"
#include "third_party/blink/public/common/features.h"
#include "ui/native_theme/native_theme.h"
#include "url/gurl.h"

namespace {
const std::string kYellow = skia::SkColorToHexString(SK_ColorYELLOW);
const std::string kGreen = skia::SkColorToHexString(SK_ColorGREEN);
const std::string kRed = skia::SkColorToHexString(SK_ColorRED);
const std::string kBlue = skia::SkColorToHexString(SK_ColorBLUE);
const std::string kBlack = skia::SkColorToHexString(SK_ColorBLACK);
const std::string kWhite = skia::SkColorToHexString(SK_ColorWHITE);
}  // namespace
// Class to test browser error page display info.
class AlternativeErrorPageOverrideInfoBrowserTest
    : public InProcessBrowserTest {
 public:
  AlternativeErrorPageOverrideInfoBrowserTest() {
    feature_list_.InitWithFeatures({features::kDesktopPWAsDefaultOfflinePage,
                                    blink::features::kWebAppEnableDarkMode},
                                   {});
  }

  // Helper function to prepare PWA and retrieve information from the
  // alternative error page function.
  content::mojom::AlternativeErrorPageOverrideInfoPtr GetErrorPageInfo(
      std::string html) {
    ChromeContentBrowserClient browser_client;
    content::ScopedContentBrowserClientSetting setting(&browser_client);

    const GURL app_url = embedded_test_server()->GetURL(html);
    web_app::NavigateToURLAndWait(browser(), app_url);
    web_app::test::InstallPwaForCurrentUrl(browser());
    content::BrowserContext* context = browser()->profile();

    return browser_client.GetAlternativeErrorPageOverrideInfo(
        app_url, context, net::ERR_INTERNET_DISCONNECTED);
  }

 private:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    InProcessBrowserTest::TearDownOnMainThread();
  }
  base::test::ScopedFeatureList feature_list_;
};

// Testing app manifest with no theme or background color.
IN_PROC_BROWSER_TEST_F(AlternativeErrorPageOverrideInfoBrowserTest, Manifest) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::mojom::AlternativeErrorPageOverrideInfoPtr info =
      GetErrorPageInfo("/banners/manifest_no_service_worker.html");

  // Expect mojom struct with default background and theme colors.
  EXPECT_TRUE(info);
  EXPECT_EQ(*info->alternative_error_page_params.FindKey(
                "customized_background_color"),
            base::Value(kWhite));
  EXPECT_EQ(*info->alternative_error_page_params.FindKey("theme_color"),
            base::Value(kBlack));
}

// Testing app manifest with theme color.
IN_PROC_BROWSER_TEST_F(AlternativeErrorPageOverrideInfoBrowserTest,
                       ManifestWithThemeColor) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::mojom::AlternativeErrorPageOverrideInfoPtr info =
      GetErrorPageInfo("/banners/theme-color.html");

  // Expect mojom struct with customized theme color and default background
  // color.
  EXPECT_TRUE(info);
  EXPECT_EQ(*info->alternative_error_page_params.FindKey(
                "customized_background_color"),
            base::Value(kWhite));
  EXPECT_EQ(
      *info->alternative_error_page_params.FindKey("theme_color"),
      base::Value(skia::SkColorToHexString(SkColorSetRGB(0xAA, 0xCC, 0xEE))));
}

// Testing app manifest with background color.
IN_PROC_BROWSER_TEST_F(AlternativeErrorPageOverrideInfoBrowserTest,
                       ManifestWithBackgroundColor) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::mojom::AlternativeErrorPageOverrideInfoPtr info =
      GetErrorPageInfo("/banners/background-color.html");

  // Expect mojom struct with default theme color and customized background
  // color.
  EXPECT_TRUE(info);
  EXPECT_EQ(*info->alternative_error_page_params.FindKey(
                "customized_background_color"),
            base::Value(kBlue));
  EXPECT_EQ(*info->alternative_error_page_params.FindKey("theme_color"),
            base::Value(kBlack));
}

// Testing url outside the scope of an installed app.
IN_PROC_BROWSER_TEST_F(AlternativeErrorPageOverrideInfoBrowserTest,
                       NoManifest) {
  ChromeContentBrowserClient browser_client;
  content::ScopedContentBrowserClientSetting setting(&browser_client);

  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url = embedded_test_server()->GetURL("/simple.html");
  content::BrowserContext* context = browser()->profile();

  content::mojom::AlternativeErrorPageOverrideInfoPtr info =
      browser_client.GetAlternativeErrorPageOverrideInfo(
          app_url, context, net::ERR_INTERNET_DISCONNECTED);

  // Expect mojom struct to be null.
  EXPECT_FALSE(info);
}

// Testing manifest with app short name.
IN_PROC_BROWSER_TEST_F(AlternativeErrorPageOverrideInfoBrowserTest,
                       ManifestWithAppShortName) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::mojom::AlternativeErrorPageOverrideInfoPtr info = GetErrorPageInfo(
      "/banners/"
      "manifest_test_page.html?manifest=manifest_short_name_only.json");

  // Expect mojom struct with custom app short name.
  EXPECT_TRUE(info);
  EXPECT_EQ(*info->alternative_error_page_params.FindKey("app_short_name"),
            base::Value("Manifest"));
}

// Testing app manifest with no app short name.
IN_PROC_BROWSER_TEST_F(AlternativeErrorPageOverrideInfoBrowserTest,
                       ManifestWithNoAppShortName) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::mojom::AlternativeErrorPageOverrideInfoPtr info = GetErrorPageInfo(
      "/banners/"
      "manifest_test_page.html?manifest=manifest.json");

  // Expect mojom struct with customized with app name.
  EXPECT_TRUE(info);
  EXPECT_EQ(*info->alternative_error_page_params.FindKey("app_short_name"),
            base::Value("Manifest test app"));
}

// Testing app manifest with no app short name or app name.
IN_PROC_BROWSER_TEST_F(AlternativeErrorPageOverrideInfoBrowserTest,
                       ManifestWithNoAppShortNameOrAppName) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::mojom::AlternativeErrorPageOverrideInfoPtr info = GetErrorPageInfo(
      "/banners/"
      "manifest_test_page.html?manifest=manifest_empty_name_short_name.json");

  // Expect mojom struct customized with HTML page title.
  EXPECT_TRUE(info);
  EXPECT_EQ(*info->alternative_error_page_params.FindKey("app_short_name"),
            base::Value("Web app banner test page"));
}

// Testing app manifest with no app short name or app name, and HTML page
// has no title
IN_PROC_BROWSER_TEST_F(AlternativeErrorPageOverrideInfoBrowserTest,
                       ManifestWithNoAppShortNameOrAppNameOrTitle) {
  ChromeContentBrowserClient browser_client;
  content::ScopedContentBrowserClientSetting setting(&browser_client);

  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url = embedded_test_server()->GetURL("/title1.html");
  web_app::NavigateToURLAndWait(browser(), app_url);
  web_app::test::InstallPwaForCurrentUrl(browser());
  content::BrowserContext* context = browser()->profile();

  content::mojom::AlternativeErrorPageOverrideInfoPtr info =
      browser_client.GetAlternativeErrorPageOverrideInfo(
          app_url, context, net::ERR_INTERNET_DISCONNECTED);

  // Expect mojom struct customized with HTML page title.
  EXPECT_TRUE(info);
  EXPECT_EQ(*info->alternative_error_page_params.FindKey("app_short_name"),
            base::Value(url_formatter::FormatUrl(app_url)));
}

// Testing app with manifest and no service worker.
IN_PROC_BROWSER_TEST_F(AlternativeErrorPageOverrideInfoBrowserTest,
                       ManifestAndNoServiceWorker) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::mojom::AlternativeErrorPageOverrideInfoPtr info =
      GetErrorPageInfo("/banners/no-sw-with-colors.html");

  // Expect mojom struct with custom theme and background color.
  EXPECT_TRUE(info);
  EXPECT_EQ(*info->alternative_error_page_params.FindKey(
                "customized_background_color"),
            base::Value(kYellow));
  EXPECT_EQ(*info->alternative_error_page_params.FindKey("theme_color"),
            base::Value(kGreen));
}

// Testing app manifest with dark mode theme and background colors.
IN_PROC_BROWSER_TEST_F(AlternativeErrorPageOverrideInfoBrowserTest,
                       ManifestWithDarkModeThemeAndBackgroundColor) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ui::NativeTheme::GetInstanceForNativeUi()->set_use_dark_colors(true);
  content::mojom::AlternativeErrorPageOverrideInfoPtr info =
      GetErrorPageInfo("/web_apps/get_manifest.html?color_scheme_dark.json");

  // Expect mojom struct with dark mode theme color and dark mode background
  // color.
  EXPECT_TRUE(info);
  EXPECT_EQ(*info->alternative_error_page_params.FindKey(
                "dark_mode_background_color"),
            base::Value(kRed));
  EXPECT_EQ(
      *info->alternative_error_page_params.FindKey("dark_mode_theme_color"),
      base::Value(kRed));
}

// Testing app manifest with no dark mode theme or background color.
IN_PROC_BROWSER_TEST_F(AlternativeErrorPageOverrideInfoBrowserTest,
                       ManifestWithNoDarkModeThemeAndBackgroundColor) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ui::NativeTheme::GetInstanceForNativeUi()->set_use_dark_colors(true);
  content::mojom::AlternativeErrorPageOverrideInfoPtr info =
      GetErrorPageInfo("/banners/no-sw-with-colors.html");

  // Expect mojom struct light mode background and theme color stored.
  EXPECT_TRUE(info);
  EXPECT_EQ(*info->alternative_error_page_params.FindKey(
                "dark_mode_background_color"),
            base::Value(kYellow));
  EXPECT_EQ(
      *info->alternative_error_page_params.FindKey("dark_mode_theme_color"),
      base::Value(kGreen));
}

// Testing manifest with icon.
IN_PROC_BROWSER_TEST_F(AlternativeErrorPageOverrideInfoBrowserTest,
                       ManifestWithIcon) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ChromeContentBrowserClient browser_client;
  content::ScopedContentBrowserClientSetting setting(&browser_client);
  const GURL app_url = embedded_test_server()->GetURL(
      "/banners/"
      "manifest_test_page.html?manifest=manifest_one_icon.json");
  web_app::NavigateToURLAndWait(browser(), app_url);
  web_app::test::InstallPwaForCurrentUrl(browser());
  Profile* profile = browser()->profile();
  web_app::WebAppProvider* web_app_provider =
      web_app::WebAppProvider::GetForTest(profile);
  const absl::optional<web_app::AppId> app_id =
      web_app_provider->registrar().FindAppWithUrlInScope(app_url);
  WebAppIconWaiter(profile, app_id.value()).Wait();
  content::mojom::AlternativeErrorPageOverrideInfoPtr info =
      browser_client.GetAlternativeErrorPageOverrideInfo(
          app_url, profile, net::ERR_INTERNET_DISCONNECTED);

  // Expect mojom struct with icon url.
  EXPECT_TRUE(info);
  EXPECT_EQ(
      *info->alternative_error_page_params.FindKey("icon_url"),
      base::Value(
          "data:image/"
          "png;base64,"
          "iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAIAAACQkWg2AAAA4UlEQVQ4jZVSuwqFMAw1"
          "8baI2M3FyUXXfoGT3+FvC/"
          "6A4iQKUrFDcweht7fWwUwhOSc5eUDXddEbw1foKIo+"
          "1iOiIAIAiAgAfIINubh7KtABALTWxhjOuYVax58BAM7zLIqirmtEvMS4an8EIkJEpZSU"
          "smkaIYQQwhjjSUK3tmVmWaaUWtc1jmNvGbGU0m3COZ/neRiGtm3zPO/"
          "7Pk3T8JYuSVrrsiyrqjqOY5omzrkx5nGtRMQYG8dx33et9bZtSZK46D+"
          "Cy1yWBREvtJcNH44xdg8GZrh3u5d7fI0ne/2tX8uNb4qnIrpWAAAAAElFTkSuQmCC"));
}
