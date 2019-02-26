// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_INSTALL_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_INSTALL_UTILS_H_

#include <map>
#include <set>
#include <vector>

#include "url/gurl.h"

struct InstallableData;
struct WebApplicationInfo;
class SkBitmap;

namespace blink {
struct Manifest;
}

namespace web_app {

struct BitmapAndSource;

enum class ForInstallableSite {
  kYes,
  kNo,
  kUnknown,
};

// A map of icon urls to the bitmaps provided by that url.
using IconsMap = std::map<GURL, std::vector<SkBitmap>>;

// Update the given WebApplicationInfo with information from the manifest.
void UpdateWebAppInfoFromManifest(const blink::Manifest& manifest,
                                  WebApplicationInfo* web_app_info,
                                  ForInstallableSite installable_site);

// Returns icon sizes to be generated from downloaded icons.
std::set<int> SizesToGenerate();

// Form a list of icons to download:
// Remove icons with invalid urls. Skip primary icon that we already have
// downloaded during installability check phase.
std::vector<GURL> GetValidIconUrlsToDownload(
    const InstallableData& data,
    const WebApplicationInfo& web_app_info);

// Merge primary icon from installability check phase:
// Add the primary icon to the final web app creation data.
void MergeInstallableDataIcon(const InstallableData& data,
                              WebApplicationInfo* web_app_info);

// Get a list of non-empty square icons from downloaded |icons_map| and
// |web_app_info| (merged together).
std::vector<BitmapAndSource> FilterSquareIcons(
    const IconsMap& icons_map,
    const WebApplicationInfo& web_app_info);

// Ensure that the necessary-sized icons are available by resizing larger
// icons down to smaller sizes, and generating icons for sizes where resizing
// is not possible.
void ResizeDownloadedIconsGenerateMissing(
    std::vector<BitmapAndSource> downloaded_icons,
    WebApplicationInfo* web_app_info);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_INSTALL_UTILS_H_
