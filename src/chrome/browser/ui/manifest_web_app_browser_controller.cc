// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/manifest_web_app_browser_controller.h"

#include "chrome/browser/ui/browser.h"
#include "content/public/browser/navigation_entry.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

ManifestWebAppBrowserController::ManifestWebAppBrowserController(
    Browser* browser)
    : WebAppBrowserController(browser) {}

ManifestWebAppBrowserController::~ManifestWebAppBrowserController() = default;

base::Optional<std::string> ManifestWebAppBrowserController::GetAppId() const {
  return base::nullopt;
}

bool ManifestWebAppBrowserController::ShouldShowToolbar() const {
  return false;
}

bool ManifestWebAppBrowserController::ShouldShowHostedAppButtonContainer()
    const {
  return true;
}

gfx::ImageSkia ManifestWebAppBrowserController::GetWindowAppIcon() const {
  gfx::ImageSkia page_icon = browser()->GetCurrentPageIcon().AsImageSkia();
  if (!page_icon.isNull())
    return page_icon;

  // The extension icon may be loading still. Return a transparent icon rather
  // than using a placeholder to avoid flickering.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(gfx::kFaviconSize, gfx::kFaviconSize);
  bitmap.eraseColor(SK_ColorTRANSPARENT);
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

gfx::ImageSkia ManifestWebAppBrowserController::GetWindowIcon() const {
  return browser()->GetCurrentPageIcon().AsImageSkia();
}

base::Optional<SkColor> ManifestWebAppBrowserController::GetThemeColor() const {
  base::Optional<SkColor> result;

  // HTML meta theme-color tag overrides manifest theme_color, see spec:
  // https://www.w3.org/TR/appmanifest/#theme_color-member
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  if (web_contents) {
    base::Optional<SkColor> color = web_contents->GetThemeColor();
    if (color)
      result = color;
  }

  if (!result)
    return base::nullopt;

  // The frame/tabstrip code expects an opaque color.
  return SkColorSetA(*result, SK_AlphaOPAQUE);
}

base::string16 ManifestWebAppBrowserController::GetTitle() const {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return base::string16();

  content::NavigationEntry* entry =
      web_contents->GetController().GetVisibleEntry();
  return entry ? entry->GetTitle() : base::string16();
}

std::string ManifestWebAppBrowserController::GetAppShortName() const {
  return std::string();
}

base::string16 ManifestWebAppBrowserController::GetFormattedUrlOrigin() const {
  return FormatUrlOrigin(GetAppLaunchURL());
}

GURL ManifestWebAppBrowserController::GetAppLaunchURL() const {
  return GURL();
}
