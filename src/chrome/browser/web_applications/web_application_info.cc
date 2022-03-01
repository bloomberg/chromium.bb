// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_application_info.h"

#include <sstream>

#include "components/webapps/common/web_page_metadata.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

namespace {

template <typename T>
std::string ConvertToString(const T& value) {
  std::stringstream ss;
  ss << value;
  return ss.str();
}

}  // namespace

apps::IconInfo::Purpose ManifestPurposeToIconInfoPurpose(
    IconPurpose manifest_purpose) {
  switch (manifest_purpose) {
    case IconPurpose::ANY:
      return apps::IconInfo::Purpose::kAny;
    case IconPurpose::MONOCHROME:
      return apps::IconInfo::Purpose::kMonochrome;
    case IconPurpose::MASKABLE:
      return apps::IconInfo::Purpose::kMaskable;
  }
}

// IconBitmaps
IconBitmaps::IconBitmaps() = default;

IconBitmaps::~IconBitmaps() = default;

IconBitmaps::IconBitmaps(const IconBitmaps&) = default;

IconBitmaps::IconBitmaps(IconBitmaps&&) noexcept = default;

IconBitmaps& IconBitmaps::operator=(const IconBitmaps&) = default;

IconBitmaps& IconBitmaps::operator=(IconBitmaps&&) noexcept = default;

const std::map<SquareSizePx, SkBitmap>& IconBitmaps::GetBitmapsForPurpose(
    IconPurpose purpose) const {
  switch (purpose) {
    case IconPurpose::MONOCHROME:
      return monochrome;
    case IconPurpose::ANY:
      return any;
    case IconPurpose::MASKABLE:
      return maskable;
  }
}

void IconBitmaps::SetBitmapsForPurpose(
    IconPurpose purpose,
    std::map<SquareSizePx, SkBitmap> bitmaps) {
  switch (purpose) {
    case IconPurpose::ANY:
      any = std::move(bitmaps);
      return;
    case IconPurpose::MONOCHROME:
      monochrome = std::move(bitmaps);
      return;
    case IconPurpose::MASKABLE:
      maskable = std::move(bitmaps);
      return;
  }
}

bool IconBitmaps::empty() const {
  return any.empty() && maskable.empty() && monochrome.empty();
}

// IconSizes
IconSizes::IconSizes() = default;

IconSizes::~IconSizes() = default;

IconSizes::IconSizes(const IconSizes&) = default;

IconSizes::IconSizes(IconSizes&&) noexcept = default;

IconSizes& IconSizes::operator=(const IconSizes&) = default;

IconSizes& IconSizes::operator=(IconSizes&&) noexcept = default;

const std::vector<SquareSizePx>& IconSizes::GetSizesForPurpose(
    IconPurpose purpose) const {
  switch (purpose) {
    case IconPurpose::MONOCHROME:
      return monochrome;
    case IconPurpose::ANY:
      return any;
    case IconPurpose::MASKABLE:
      return maskable;
  }
}

void IconSizes::SetSizesForPurpose(IconPurpose purpose,
                                   std::vector<SquareSizePx> sizes) {
  switch (purpose) {
    case IconPurpose::ANY:
      any = std::move(sizes);
      return;
    case IconPurpose::MONOCHROME:
      monochrome = std::move(sizes);
      return;
    case IconPurpose::MASKABLE:
      maskable = std::move(sizes);
      return;
  }
}

bool IconSizes::empty() const {
  return any.empty() && maskable.empty() && monochrome.empty();
}

// WebAppShortcutsMenuItemInfo::Icon
WebAppShortcutsMenuItemInfo::Icon::Icon() = default;

WebAppShortcutsMenuItemInfo::Icon::Icon(
    const WebAppShortcutsMenuItemInfo::Icon&) = default;

WebAppShortcutsMenuItemInfo::Icon::Icon(WebAppShortcutsMenuItemInfo::Icon&&) =
    default;

WebAppShortcutsMenuItemInfo::Icon::~Icon() = default;

WebAppShortcutsMenuItemInfo::Icon& WebAppShortcutsMenuItemInfo::Icon::operator=(
    const WebAppShortcutsMenuItemInfo::Icon&) = default;

WebAppShortcutsMenuItemInfo::Icon& WebAppShortcutsMenuItemInfo::Icon::operator=(
    WebAppShortcutsMenuItemInfo::Icon&&) = default;

base::Value WebAppShortcutsMenuItemInfo::Icon::AsDebugValue() const {
  base::Value root(base::Value::Type::DICTIONARY);
  root.SetStringKey("url", url.spec());
  root.SetIntKey("square_size_px", square_size_px);
  return root;
}

// WebAppShortcutsMenuItemInfo
WebAppShortcutsMenuItemInfo::WebAppShortcutsMenuItemInfo() = default;

WebAppShortcutsMenuItemInfo::WebAppShortcutsMenuItemInfo(
    const WebAppShortcutsMenuItemInfo& other) = default;

WebAppShortcutsMenuItemInfo::WebAppShortcutsMenuItemInfo(
    WebAppShortcutsMenuItemInfo&&) noexcept = default;

WebAppShortcutsMenuItemInfo::~WebAppShortcutsMenuItemInfo() = default;

WebAppShortcutsMenuItemInfo& WebAppShortcutsMenuItemInfo::operator=(
    const WebAppShortcutsMenuItemInfo&) = default;

WebAppShortcutsMenuItemInfo& WebAppShortcutsMenuItemInfo::operator=(
    WebAppShortcutsMenuItemInfo&&) noexcept = default;

const std::vector<WebAppShortcutsMenuItemInfo::Icon>&
WebAppShortcutsMenuItemInfo::GetShortcutIconInfosForPurpose(
    IconPurpose purpose) const {
  switch (purpose) {
    case IconPurpose::MONOCHROME:
      return monochrome;
    case IconPurpose::ANY:
      return any;
    case IconPurpose::MASKABLE:
      return maskable;
  }
}

void WebAppShortcutsMenuItemInfo::SetShortcutIconInfosForPurpose(
    IconPurpose purpose,
    std::vector<WebAppShortcutsMenuItemInfo::Icon> shortcut_manifest_icons) {
  switch (purpose) {
    case IconPurpose::ANY:
      any = std::move(shortcut_manifest_icons);
      return;
    case IconPurpose::MONOCHROME:
      monochrome = std::move(shortcut_manifest_icons);
      return;
    case IconPurpose::MASKABLE:
      maskable = std::move(shortcut_manifest_icons);
      return;
  }
}

base::Value WebAppShortcutsMenuItemInfo::AsDebugValue() const {
  base::Value root(base::Value::Type::DICTIONARY);

  root.SetStringKey("name", name);

  root.SetStringKey("url", url.spec());

  base::Value& icons =
      *root.SetKey("icons", base::Value(base::Value::Type::DICTIONARY));
  for (IconPurpose purpose : kIconPurposes) {
    base::Value& purpose_list = *icons.SetKey(
        ConvertToString(purpose), base::Value(base::Value::Type::LIST));
    for (const WebAppShortcutsMenuItemInfo::Icon& icon :
         GetShortcutIconInfosForPurpose(purpose)) {
      purpose_list.Append(icon.AsDebugValue());
    }
  }

  return root;
}

// WebApplicationInfo
WebApplicationInfo::WebApplicationInfo() = default;

WebApplicationInfo::WebApplicationInfo(const WebApplicationInfo& other) =
    default;

WebApplicationInfo::WebApplicationInfo(
    const webapps::mojom::WebPageMetadata& metadata)
    : title(metadata.application_name),
      description(metadata.description),
      start_url(metadata.application_url) {
  for (const auto& icon : metadata.icons) {
    apps::IconInfo icon_info;
    icon_info.url = icon->url;
    if (icon->square_size_px > 0)
      icon_info.square_size_px = icon->square_size_px;
    manifest_icons.push_back(icon_info);
  }
  switch (metadata.mobile_capable) {
    case webapps::mojom::WebPageMobileCapable::UNSPECIFIED:
      mobile_capable = MOBILE_CAPABLE_UNSPECIFIED;
      break;
    case webapps::mojom::WebPageMobileCapable::ENABLED:
      mobile_capable = MOBILE_CAPABLE;
      break;
    case webapps::mojom::WebPageMobileCapable::ENABLED_APPLE:
      mobile_capable = MOBILE_CAPABLE_APPLE;
      break;
  }
}

WebApplicationInfo::~WebApplicationInfo() = default;

bool operator==(const IconSizes& icon_sizes1, const IconSizes& icon_sizes2) {
  return std::tie(icon_sizes1.any, icon_sizes1.maskable,
                  icon_sizes1.monochrome) == std::tie(icon_sizes2.any,
                                                      icon_sizes2.maskable,
                                                      icon_sizes2.monochrome);
}

bool operator==(const WebAppShortcutsMenuItemInfo::Icon& icon1,
                const WebAppShortcutsMenuItemInfo::Icon& icon2) {
  return std::tie(icon1.url, icon1.square_size_px) ==
         std::tie(icon2.url, icon2.square_size_px);
}

bool operator==(const WebAppShortcutsMenuItemInfo& shortcut_info1,
                const WebAppShortcutsMenuItemInfo& shortcut_info2) {
  return std::tie(shortcut_info1.name, shortcut_info1.url, shortcut_info1.any,
                  shortcut_info1.maskable, shortcut_info1.monochrome) ==
         std::tie(shortcut_info2.name, shortcut_info2.url, shortcut_info2.any,
                  shortcut_info2.maskable, shortcut_info2.monochrome);
}
