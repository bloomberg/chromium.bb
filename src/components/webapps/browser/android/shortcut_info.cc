// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/shortcut_info.h"

#include <string>

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "components/webapps/browser/android/webapps_icon_utils.h"
#include "shortcut_info.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/manifest/manifest_icon_selector.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

namespace webapps {

namespace {

// The maximum number of shortcuts an Android launcher supports.
// https://developer.android.com/guide/topics/ui/shortcuts#shortcut-limitations
constexpr size_t kMaxShortcuts = 4;

}  // namespace

ShareTargetParamsFile::ShareTargetParamsFile() {}

ShareTargetParamsFile::ShareTargetParamsFile(
    const ShareTargetParamsFile& other) = default;

ShareTargetParamsFile::~ShareTargetParamsFile() {}

ShareTargetParams::ShareTargetParams() {}

ShareTargetParams::ShareTargetParams(const ShareTargetParams& other) = default;

ShareTargetParams::~ShareTargetParams() {}

ShareTarget::ShareTarget() {}

ShareTarget::~ShareTarget() {}

ShortcutInfo::ShortcutInfo(const GURL& shortcut_url) : url(shortcut_url) {}

ShortcutInfo::ShortcutInfo(const ShortcutInfo& other) = default;

ShortcutInfo::~ShortcutInfo() = default;

// static
std::unique_ptr<ShortcutInfo> ShortcutInfo::CreateShortcutInfo(
    const GURL& manifest_url,
    const blink::mojom::Manifest& manifest,
    const GURL& primary_icon_url) {
  auto shortcut_info = std::make_unique<ShortcutInfo>(GURL());
  if (!blink::IsEmptyManifest(manifest)) {
    shortcut_info->UpdateFromManifest(manifest);
    shortcut_info->manifest_url = manifest_url;
    shortcut_info->best_primary_icon_url = primary_icon_url;
    shortcut_info->UpdateBestSplashIcon(manifest);
  }

  return shortcut_info;
}

void ShortcutInfo::UpdateFromManifest(const blink::mojom::Manifest& manifest) {
  std::u16string s_name = manifest.short_name.value_or(std::u16string());
  std::u16string f_name = manifest.name.value_or(std::u16string());
  if (!s_name.empty() || !f_name.empty()) {
    short_name = s_name;
    name = f_name;
    if (short_name.empty())
      short_name = name;
    else if (name.empty())
      name = short_name;
  }
  user_title = short_name;

  description = manifest.description.value_or(std::u16string());

  // Set the url based on the manifest value, if any.
  if (manifest.start_url.is_valid())
    url = manifest.start_url;

  scope = manifest.scope;

  // Set the display based on the manifest value, if any.
  if (manifest.display != blink::mojom::DisplayMode::kUndefined)
    display = manifest.display;

  if (display == blink::mojom::DisplayMode::kStandalone ||
      display == blink::mojom::DisplayMode::kFullscreen ||
      display == blink::mojom::DisplayMode::kMinimalUi) {
    source = SOURCE_ADD_TO_HOMESCREEN_STANDALONE;
    // Set the orientation based on the manifest value, or ignore if the display
    // mode is different from 'standalone', 'fullscreen' or 'minimal-ui'.
    if (manifest.orientation !=
        device::mojom::ScreenOrientationLockType::DEFAULT) {
      // TODO(mlamouri): Send a message to the developer console if we ignored
      // Manifest orientation because display property is not set.
      orientation = manifest.orientation;
    }
  }

  // Set the theme color based on the manifest value, if any.
  theme_color = manifest.has_theme_color
                    ? absl::make_optional(manifest.theme_color)
                    : absl::nullopt;

  // Set the background color based on the manifest value, if any.
  background_color = manifest.has_background_color
                         ? absl::make_optional(manifest.background_color)
                         : absl::nullopt;

  // Set the icon urls based on the icons in the manifest, if any.
  icon_urls.clear();
  for (const auto& icon : manifest.icons)
    icon_urls.push_back(icon.src.spec());

  // Set the screenshots urls based on the screenshots in the manifest, if any.
  screenshot_urls.clear();
  for (const auto& screenshot : manifest.screenshots)
    screenshot_urls.push_back(screenshot.src);

  if (manifest.share_target) {
    share_target = ShareTarget();
    share_target->action = manifest.share_target->action;
    share_target->method = manifest.share_target->method;
    share_target->enctype = manifest.share_target->enctype;
    if (manifest.share_target->params.text)
      share_target->params.text = *manifest.share_target->params.text;
    if (manifest.share_target->params.title)
      share_target->params.title = *manifest.share_target->params.title;
    if (manifest.share_target->params.url)
      share_target->params.url = *manifest.share_target->params.url;

    for (blink::Manifest::FileFilter manifest_share_target_file :
         manifest.share_target->params.files) {
      ShareTargetParamsFile share_target_params_file;
      share_target_params_file.name = manifest_share_target_file.name;
      share_target_params_file.accept = manifest_share_target_file.accept;
      share_target->params.files.push_back(share_target_params_file);
    }
  }

  shortcut_items = manifest.shortcuts;
  if (shortcut_items.size() > kMaxShortcuts)
    shortcut_items.resize(kMaxShortcuts);

  for (auto& shortcut_item : shortcut_items) {
    if (!shortcut_item.short_name || shortcut_item.short_name->empty())
      shortcut_item.short_name = shortcut_item.name;
  }

  int ideal_shortcut_icons_size_px =
      WebappsIconUtils::GetIdealShortcutIconSizeInPx();
  for (const auto& manifest_shortcut : shortcut_items) {
    GURL best_url = blink::ManifestIconSelector::FindBestMatchingSquareIcon(
        manifest_shortcut.icons, ideal_shortcut_icons_size_px,
        /* minimum_icon_size_in_px= */ ideal_shortcut_icons_size_px / 2,
        blink::mojom::ManifestImageResource_Purpose::ANY);
    best_shortcut_icon_urls.push_back(std::move(best_url));
  }
}

void ShortcutInfo::UpdateBestSplashIcon(
    const blink::mojom::Manifest& manifest) {
  ideal_splash_image_size_in_px =
      WebappsIconUtils::GetIdealSplashImageSizeInPx();
  minimum_splash_image_size_in_px =
      WebappsIconUtils::GetMinimumSplashImageSizeInPx();

  if (WebappsIconUtils::DoesAndroidSupportMaskableIcons()) {
    splash_image_url = blink::ManifestIconSelector::FindBestMatchingSquareIcon(
        manifest.icons, ideal_splash_image_size_in_px,
        minimum_splash_image_size_in_px,
        blink::mojom::ManifestImageResource_Purpose::MASKABLE);
    is_splash_image_maskable = true;
  }
  // If did not fetch maskable icon for splash image, or can not find a best
  // match, fallback to ANY icon.
  if (!splash_image_url.is_valid()) {
    splash_image_url = blink::ManifestIconSelector::FindBestMatchingSquareIcon(
        manifest.icons, ideal_splash_image_size_in_px,
        minimum_splash_image_size_in_px,
        blink::mojom::ManifestImageResource_Purpose::ANY);
    is_splash_image_maskable = false;
  }
}

void ShortcutInfo::UpdateSource(const Source new_source) {
  source = new_source;
}

}  // namespace webapps
