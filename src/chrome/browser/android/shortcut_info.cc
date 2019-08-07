// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/shortcut_info.h"

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"

ShareTargetParamsFile::ShareTargetParamsFile() {}

ShareTargetParamsFile::ShareTargetParamsFile(
    const ShareTargetParamsFile& other) = default;

ShareTargetParamsFile::~ShareTargetParamsFile() {}

ShareTargetParams::ShareTargetParams() {}

ShareTargetParams::ShareTargetParams(const ShareTargetParams& other) = default;

ShareTargetParams::~ShareTargetParams() {}

ShareTarget::ShareTarget() {}

ShareTarget::~ShareTarget() {}

ShortcutInfo::ShortcutInfo(const GURL& shortcut_url)
    : url(shortcut_url),
      display(blink::kWebDisplayModeBrowser),
      orientation(blink::kWebScreenOrientationLockDefault),
      source(SOURCE_ADD_TO_HOMESCREEN_SHORTCUT),
      ideal_splash_image_size_in_px(0),
      minimum_splash_image_size_in_px(0) {}

ShortcutInfo::ShortcutInfo(const ShortcutInfo& other) = default;

ShortcutInfo::~ShortcutInfo() {
}

void ShortcutInfo::UpdateFromManifest(const blink::Manifest& manifest) {
  if (!manifest.short_name.string().empty() ||
      !manifest.name.string().empty()) {
    short_name = manifest.short_name.string();
    name = manifest.name.string();
    if (short_name.empty())
      short_name = name;
    else if (name.empty())
      name = short_name;
  }
  user_title = short_name;

  // Set the url based on the manifest value, if any.
  if (manifest.start_url.is_valid())
    url = manifest.start_url;

  scope = manifest.scope;

  // Set the display based on the manifest value, if any.
  if (manifest.display != blink::kWebDisplayModeUndefined)
    display = manifest.display;

  if (display == blink::kWebDisplayModeStandalone ||
      display == blink::kWebDisplayModeFullscreen ||
      display == blink::kWebDisplayModeMinimalUi) {
    source = SOURCE_ADD_TO_HOMESCREEN_STANDALONE;
    // Set the orientation based on the manifest value, or ignore if the display
    // mode is different from 'standalone', 'fullscreen' or 'minimal-ui'.
    if (manifest.orientation != blink::kWebScreenOrientationLockDefault) {
      // TODO(mlamouri): Send a message to the developer console if we ignored
      // Manifest orientation because display property is not set.
      orientation = manifest.orientation;
    }
  }

  // Set the theme color based on the manifest value, if any.
  if (manifest.theme_color)
    theme_color = manifest.theme_color;

  // Set the background color based on the manifest value, if any.
  if (manifest.background_color)
    background_color = manifest.background_color;

  // Sets the URL of the HTML splash screen, if any.
  if (manifest.splash_screen_url.is_valid())
    splash_screen_url = manifest.splash_screen_url;

  // Set the icon urls based on the icons in the manifest, if any.
  icon_urls.clear();
  for (const auto& icon : manifest.icons)
    icon_urls.push_back(icon.src.spec());

  if (manifest.share_target) {
    share_target = ShareTarget();
    share_target->action = manifest.share_target->action;
    if (manifest.share_target->method ==
        blink::Manifest::ShareTarget::Method::kPost) {
      share_target->method = ShareTarget::Method::kPost;
    } else {
      share_target->method = ShareTarget::Method::kGet;
    }
    if (manifest.share_target->enctype ==
        blink::Manifest::ShareTarget::Enctype::kMultipart) {
      share_target->enctype = ShareTarget::Enctype::kMultipart;
    } else {
      share_target->enctype = ShareTarget::Enctype::kApplication;
    }
    if (!manifest.share_target->params.text.is_null())
      share_target->params.text = manifest.share_target->params.text.string();
    if (!manifest.share_target->params.title.is_null())
      share_target->params.title = manifest.share_target->params.title.string();
    if (!manifest.share_target->params.url.is_null())
      share_target->params.url = manifest.share_target->params.url.string();

    for (blink::Manifest::FileFilter manifest_share_target_file :
         manifest.share_target->params.files) {
      ShareTargetParamsFile share_target_params_file;
      share_target_params_file.name = manifest_share_target_file.name;
      share_target_params_file.accept = manifest_share_target_file.accept;
      share_target->params.files.push_back(share_target_params_file);
    }
  }
}

void ShortcutInfo::UpdateSource(const Source new_source) {
  source = new_source;
}
