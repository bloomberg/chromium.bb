// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_install_utils.h"

#include <string>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/os_integration/web_app_file_handler_manager.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/skia_util.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_WIN)
#include "base/test/bind.h"
#include "chrome/browser/web_applications/test/mock_os_integration_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock-actions.h"
#include "testing/gmock/include/gmock/gmock.h"

#endif
namespace web_app {

using Purpose = blink::mojom::ManifestImageResource_Purpose;

namespace {

const char16_t kAppTestShortName[] = u"Test short name";
const char16_t kAppTestTitle[] = u"Test title";
const char16_t kAlternativeAppTestTitle[] = u"Different test title";
const char16_t kShortcutItemTestName[] = u"shortcut item ";

constexpr SquareSizePx kIconSize = 64;

// This value is greater than kMaxIcons in web_app_install_utils.cc.
constexpr unsigned int kNumTestIcons = 30;

IconPurpose IconInfoPurposeToManifestPurpose(
    apps::IconInfo::Purpose icon_info_purpose) {
  switch (icon_info_purpose) {
    case apps::IconInfo::Purpose::kAny:
      return IconPurpose::ANY;
    case apps::IconInfo::Purpose::kMonochrome:
      return IconPurpose::MONOCHROME;
    case apps::IconInfo::Purpose::kMaskable:
      return IconPurpose::MASKABLE;
  }
}

}  // namespace

TEST(WebAppInstallUtils, UpdateWebAppInfoFromManifest) {
  base::test::ScopedFeatureList feature_list(
      blink::features::kFileHandlingIcons);

  WebAppInstallInfo web_app_info;
  web_app_info.title = kAlternativeAppTestTitle;
  web_app_info.start_url = GURL("http://www.notchromium.org");
  apps::IconInfo info;
  const GURL kAppIcon1("fav1.png");
  info.url = kAppIcon1;
  web_app_info.manifest_icons.push_back(info);

  blink::mojom::Manifest manifest;
  const GURL kAppUrl("http://www.chromium.org/index.html");
  manifest.start_url = kAppUrl;
  manifest.scope = kAppUrl.GetWithoutFilename();
  manifest.short_name = kAppTestShortName;

  const GURL kFileHandlingIcon("fav1.png");
  {
    auto handler = blink::mojom::ManifestFileHandler::New();
    handler->action = GURL("http://example.com/open-files");
    handler->accept[u"image/png"].push_back(u".png");
    handler->name = u"Images";
    {
      blink::Manifest::ImageResource icon;
      icon.src = kFileHandlingIcon;
      icon.purpose = {Purpose::ANY, Purpose::MONOCHROME};
      handler->icons.push_back(icon);
    }
    manifest.file_handlers.push_back(std::move(handler));
  }

  {
    auto protocol_handler = blink::mojom::ManifestProtocolHandler::New();
    protocol_handler->protocol = u"mailto";
    protocol_handler->url = GURL("http://example.com/handle=%s");
    manifest.protocol_handlers.push_back(std::move(protocol_handler));
  }

  {
    auto url_handler = blink::mojom::ManifestUrlHandler::New();
    url_handler->origin =
        url::Origin::Create(GURL("https://url_handlers_origin.com/"));
    url_handler->has_origin_wildcard = false;
    manifest.url_handlers.push_back(std::move(url_handler));
  }

  {
    auto declaration =
        blink::mojom::ManifestPermissionsPolicyDeclaration::New();
    declaration->feature = "bluetooth";
    declaration->allowlist = {"*"};
    manifest.permissions_policy.push_back(std::move(declaration));
  }

  {
    // Ensure an empty NoteTaking struct is ignored.
    manifest.note_taking = blink::mojom::ManifestNoteTaking::New();
  }

  const GURL kAppManifestUrl("http://www.chromium.org/manifest.json");
  UpdateWebAppInfoFromManifest(manifest, kAppManifestUrl, &web_app_info);
  EXPECT_EQ(kAppTestShortName, web_app_info.title);
  EXPECT_EQ(kAppUrl, web_app_info.start_url);
  EXPECT_EQ(kAppUrl.GetWithoutFilename(), web_app_info.scope);
  EXPECT_EQ(DisplayMode::kBrowser, web_app_info.display_mode);
  EXPECT_TRUE(web_app_info.display_override.empty());
  EXPECT_EQ(kAppManifestUrl, web_app_info.manifest_url);
  EXPECT_TRUE(web_app_info.note_taking_new_note_url.is_empty());

  // The icon info from |web_app_info| should be left as is, since the manifest
  // doesn't have any icon information.
  EXPECT_EQ(1u, web_app_info.manifest_icons.size());
  EXPECT_EQ(kAppIcon1, web_app_info.manifest_icons[0].url);

  // Test that |manifest.name| takes priority over |manifest.short_name|, and
  // that icons provided by the manifest replace icons in |web_app_info|.
  manifest.name = kAppTestTitle;
  manifest.display = DisplayMode::kMinimalUi;

  blink::Manifest::ImageResource icon;

  const GURL kAppIcon2("fav2.png");
  icon.src = kAppIcon2;
  icon.purpose = {Purpose::ANY, Purpose::MONOCHROME};
  manifest.icons.push_back(icon);

  const GURL kAppIcon3("fav3.png");
  icon.src = kAppIcon3;
  icon.purpose = {Purpose::ANY, Purpose::MONOCHROME};
  manifest.icons.push_back(icon);

  // Add an icon without purpose ANY (expect to be ignored).
  icon.purpose = {Purpose::MONOCHROME};
  manifest.icons.push_back(icon);

  manifest.display_override.push_back(DisplayMode::kMinimalUi);
  manifest.display_override.push_back(DisplayMode::kStandalone);

  {
    // Update with a valid new_note_url.
    auto note_taking = blink::mojom::ManifestNoteTaking::New();
    note_taking->new_note_url = GURL("http://example.com/new-note-url");
    manifest.note_taking = std::move(note_taking);
  }

  UpdateWebAppInfoFromManifest(manifest, kAppManifestUrl, &web_app_info);
  EXPECT_EQ(kAppTestTitle, web_app_info.title);
  EXPECT_EQ(DisplayMode::kMinimalUi, web_app_info.display_mode);
  ASSERT_EQ(2u, web_app_info.display_override.size());
  EXPECT_EQ(DisplayMode::kMinimalUi, web_app_info.display_override[0]);
  EXPECT_EQ(DisplayMode::kStandalone, web_app_info.display_override[1]);

  // We currently duplicate the app icons with multiple Purposes.
  EXPECT_EQ(5u, web_app_info.manifest_icons.size());
  EXPECT_EQ(kAppIcon2, web_app_info.manifest_icons[0].url);
  EXPECT_EQ(kAppIcon3, web_app_info.manifest_icons[1].url);
  EXPECT_EQ(kAppIcon2, web_app_info.manifest_icons[2].url);
  EXPECT_EQ(kAppIcon3, web_app_info.manifest_icons[3].url);
  EXPECT_EQ(kAppIcon3, web_app_info.manifest_icons[4].url);

  // Check file handlers were updated.
  ASSERT_EQ(1u, web_app_info.file_handlers.size());
  auto file_handler = web_app_info.file_handlers[0];
  ASSERT_EQ(1u, file_handler.accept.size());
  EXPECT_EQ(file_handler.accept[0].mime_type, "image/png");
  EXPECT_EQ(manifest.file_handlers[0]->action, file_handler.action);
  EXPECT_TRUE(file_handler.accept[0].file_extensions.contains(".png"));

  // Check protocol handlers were updated.
  EXPECT_EQ(1u, web_app_info.protocol_handlers.size());
  auto protocol_handler = web_app_info.protocol_handlers[0];
  EXPECT_EQ(protocol_handler.protocol, "mailto");
  EXPECT_EQ(protocol_handler.url, GURL("http://example.com/handle=%s"));

  // Check URL handlers were updated.
  EXPECT_EQ(1u, web_app_info.url_handlers.size());
  auto url_handler = web_app_info.url_handlers[0];
  EXPECT_EQ(url_handler.origin,
            url::Origin::Create(GURL("https://url_handlers_origin.com/")));
  EXPECT_FALSE(url_handler.has_origin_wildcard);
  EXPECT_EQ(GURL("http://example.com/new-note-url"),
            web_app_info.note_taking_new_note_url);

  // Check permissions policy was updated.
  EXPECT_EQ(1u, web_app_info.permissions_policy.size());
  auto declaration = web_app_info.permissions_policy[0];
  EXPECT_EQ(declaration.feature, "bluetooth");
  EXPECT_EQ(1u, declaration.allowlist.size());
  EXPECT_EQ("*", declaration.allowlist[0]);
}

TEST(WebAppInstallUtils, UpdateWebAppInfoFromManifest_EmptyName) {
  WebAppInstallInfo web_app_info;

  blink::mojom::Manifest manifest;
  manifest.name = absl::nullopt;
  manifest.short_name = kAppTestShortName;

  UpdateWebAppInfoFromManifest(
      manifest, GURL("http://www.chromium.org/manifest.json"), &web_app_info);
  EXPECT_EQ(kAppTestShortName, web_app_info.title);
}

// Test that maskable icons are parsed as separate manifest_icons from the
// manifest.
TEST(WebAppInstallUtils, UpdateWebAppInfoFromManifest_MaskableIcon) {
  blink::mojom::Manifest manifest;
  blink::Manifest::ImageResource icon;
  icon.src = GURL("fav1.png");
  // Produces 2 separate manifest_icons.
  icon.purpose = {Purpose::ANY, Purpose::MASKABLE};
  manifest.icons.push_back(icon);
  // Produces 1 icon_info.
  icon.purpose = {Purpose::MASKABLE};
  manifest.icons.push_back(icon);
  // Produces 1 icon_info.
  icon.purpose = {Purpose::MONOCHROME};
  manifest.icons.push_back(icon);
  WebAppInstallInfo web_app_info;

  UpdateWebAppInfoFromManifest(
      manifest, GURL("http://www.chromium.org/manifest.json"), &web_app_info);
  EXPECT_EQ(4u, web_app_info.manifest_icons.size());
  std::map<IconPurpose, int> purpose_to_count;
  for (const auto& icon_info : web_app_info.manifest_icons) {
    purpose_to_count[IconInfoPurposeToManifestPurpose(icon_info.purpose)]++;
  }
  EXPECT_EQ(1, purpose_to_count[IconPurpose::ANY]);
  EXPECT_EQ(1, purpose_to_count[IconPurpose::MONOCHROME]);
  EXPECT_EQ(2, purpose_to_count[IconPurpose::MASKABLE]);
}

TEST(WebAppInstallUtils,
     UpdateWebAppInfoFromManifest_MaskableIconOnly_UsesManifestIcons) {
  blink::mojom::Manifest manifest;
  blink::Manifest::ImageResource icon;
  icon.src = GURL("fav1.png");
  icon.purpose = {Purpose::MASKABLE};
  manifest.icons.push_back(icon);
  // WebAppInstallInfo has existing icons (simulating found in page metadata).
  WebAppInstallInfo web_app_info;
  apps::IconInfo icon_info;
  web_app_info.manifest_icons.push_back(icon_info);
  web_app_info.manifest_icons.push_back(icon_info);

  UpdateWebAppInfoFromManifest(
      manifest, GURL("http://www.chromium.org/manifest.json"), &web_app_info);
  // Metadata icons are replaced by manifest icon.
  EXPECT_EQ(1U, web_app_info.manifest_icons.size());
}

TEST(WebAppInstallUtils, UpdateWebAppInfoFromManifest_ShareTarget) {
  blink::mojom::Manifest manifest;
  WebAppInstallInfo web_app_info;

  {
    blink::Manifest::ShareTarget share_target;
    share_target.action = GURL("http://example.com/share1");
    share_target.method = blink::mojom::ManifestShareTarget_Method::kPost;
    share_target.enctype =
        blink::mojom::ManifestShareTarget_Enctype::kMultipartFormData;
    share_target.params.title = u"kTitle";
    share_target.params.text = u"kText";

    blink::Manifest::FileFilter file_filter;
    file_filter.name = u"kImages";
    file_filter.accept.push_back(u".png");
    file_filter.accept.push_back(u"image/png");
    share_target.params.files.push_back(std::move(file_filter));

    manifest.share_target = std::move(share_target);
  }

  const GURL kAppManifestUrl("http://www.chromium.org/manifest.json");
  UpdateWebAppInfoFromManifest(manifest, kAppManifestUrl, &web_app_info);

  {
    EXPECT_TRUE(web_app_info.share_target.has_value());
    const auto& share_target = *web_app_info.share_target;
    EXPECT_EQ(share_target.action, GURL("http://example.com/share1"));
    EXPECT_EQ(share_target.method, apps::ShareTarget::Method::kPost);
    EXPECT_EQ(share_target.enctype,
              apps::ShareTarget::Enctype::kMultipartFormData);
    EXPECT_EQ(share_target.params.title, "kTitle");
    EXPECT_EQ(share_target.params.text, "kText");
    EXPECT_TRUE(share_target.params.url.empty());
    EXPECT_EQ(share_target.params.files.size(), 1U);
    EXPECT_EQ(share_target.params.files[0].name, "kImages");
    EXPECT_EQ(share_target.params.files[0].accept.size(), 2U);
    EXPECT_EQ(share_target.params.files[0].accept[0], ".png");
    EXPECT_EQ(share_target.params.files[0].accept[1], "image/png");
  }

  {
    blink::Manifest::ShareTarget share_target;
    share_target.action = GURL("http://example.com/share2");
    share_target.method = blink::mojom::ManifestShareTarget_Method::kGet;
    share_target.enctype =
        blink::mojom::ManifestShareTarget_Enctype::kFormUrlEncoded;
    share_target.params.text = u"kText";
    share_target.params.url = u"kUrl";

    manifest.share_target = std::move(share_target);
  }

  UpdateWebAppInfoFromManifest(manifest, kAppManifestUrl, &web_app_info);

  {
    EXPECT_TRUE(web_app_info.share_target.has_value());
    const auto& share_target = *web_app_info.share_target;
    EXPECT_EQ(share_target.action, GURL("http://example.com/share2"));
    EXPECT_EQ(share_target.method, apps::ShareTarget::Method::kGet);
    EXPECT_EQ(share_target.enctype,
              apps::ShareTarget::Enctype::kFormUrlEncoded);
    EXPECT_TRUE(share_target.params.title.empty());
    EXPECT_EQ(share_target.params.text, "kText");
    EXPECT_EQ(share_target.params.url, "kUrl");
    EXPECT_TRUE(share_target.params.files.empty());
  }

  manifest.share_target = absl::nullopt;
  UpdateWebAppInfoFromManifest(manifest, kAppManifestUrl, &web_app_info);
  EXPECT_FALSE(web_app_info.share_target.has_value());
}

// Tests that WebAppInfo is correctly updated when Manifest contains Shortcuts.
TEST(WebAppInstallUtils, UpdateWebAppInfoFromManifestWithShortcuts) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({blink::features::kFileHandlingIcons}, {});

  WebAppInstallInfo web_app_info;
  web_app_info.title = kAlternativeAppTestTitle;
  web_app_info.start_url = GURL("http://www.notchromium.org");
  apps::IconInfo info;
  const GURL kAppIcon1("fav1.png");
  info.url = kAppIcon1;
  web_app_info.manifest_icons.push_back(info);

  blink::mojom::Manifest manifest;
  const GURL kAppUrl("http://www.chromium.org/index.html");
  manifest.start_url = kAppUrl;
  manifest.scope = kAppUrl.GetWithoutFilename();
  manifest.short_name = kAppTestShortName;

  const GURL kFileHandlingIcon("fav1.png");
  {
    auto handler = blink::mojom::ManifestFileHandler::New();
    handler->action = GURL("http://example.com/open-files");
    handler->accept[u"image/png"].push_back(u".png");
    handler->name = u"Images";
    {
      blink::Manifest::ImageResource icon;
      icon.src = kFileHandlingIcon;
      icon.purpose = {Purpose::ANY, Purpose::MONOCHROME};
      handler->icons.push_back(icon);
    }
    manifest.file_handlers.push_back(std::move(handler));
  }

  {
    auto protocol_handler = blink::mojom::ManifestProtocolHandler::New();
    protocol_handler->protocol = u"mailto";
    protocol_handler->url = GURL("http://example.com/handle=%s");
    manifest.protocol_handlers.push_back(std::move(protocol_handler));
  }

  {
    auto url_handler = blink::mojom::ManifestUrlHandler::New();
    url_handler->origin =
        url::Origin::Create(GURL("https://url_handlers_origin.com/"));
    url_handler->has_origin_wildcard = true;
    manifest.url_handlers.push_back(std::move(url_handler));
  }
  WebAppInstallInfo web_app_info_original{web_app_info};

  const GURL kAppManifestUrl("http://www.chromium.org/manifest.json");
  UpdateWebAppInfoFromManifest(manifest, kAppManifestUrl, &web_app_info);
  EXPECT_EQ(kAppTestShortName, web_app_info.title);
  EXPECT_EQ(kAppUrl, web_app_info.start_url);
  EXPECT_EQ(kAppUrl.GetWithoutFilename(), web_app_info.scope);
  EXPECT_EQ(DisplayMode::kBrowser, web_app_info.display_mode);

  // The icon info from |web_app_info| should be left as is, since the manifest
  // doesn't have any icon information.
  EXPECT_EQ(1u, web_app_info.manifest_icons.size());
  EXPECT_EQ(kAppIcon1, web_app_info.manifest_icons[0].url);

  EXPECT_EQ(0u, web_app_info.shortcuts_menu_item_infos.size());
  EXPECT_EQ(web_app_info_original.shortcuts_menu_item_infos,
            web_app_info.shortcuts_menu_item_infos);

  // Test that |manifest.name| takes priority over |manifest.short_name|, and
  // that icons provided by the manifest replace icons in |web_app_info|.
  manifest.name = kAppTestTitle;
  manifest.display = DisplayMode::kMinimalUi;

  blink::Manifest::ImageResource icon;

  const GURL kAppIcon2("fav2.png");
  icon.src = kAppIcon2;
  icon.purpose = {Purpose::ANY, Purpose::MONOCHROME};
  manifest.icons.push_back(icon);

  const GURL kAppIcon3("fav3.png");
  icon.src = kAppIcon3;
  icon.purpose = {Purpose::ANY, Purpose::MONOCHROME};
  manifest.icons.push_back(icon);

  // Add an icon without purpose ANY (expect to be ignored).
  icon.purpose = {Purpose::MONOCHROME};
  manifest.icons.push_back(icon);

  // Test that shortcuts in the manifest replace those in |web_app_info|.
  const GURL kShortcutItemUrl("http://www.chromium.org/shortcuts/action");
  blink::Manifest::ShortcutItem shortcut_item;
  shortcut_item.name = std::u16string(kShortcutItemTestName) + u"4";
  shortcut_item.url = kShortcutItemUrl;

  const GURL kIconUrl2("http://www.chromium.org/shortcuts/icon2.png");
  icon.src = kIconUrl2;
  icon.sizes.emplace_back(10, 10);
  icon.purpose = {Purpose::ANY};
  shortcut_item.icons.push_back(icon);

  manifest.shortcuts.push_back(shortcut_item);

  shortcut_item.name = std::u16string(kShortcutItemTestName) + u"5";

  const GURL kIconUrl3("http://www.chromium.org/shortcuts/icon3.png");
  icon.src = kIconUrl3;
  icon.purpose = {Purpose::MASKABLE, Purpose::MONOCHROME};

  shortcut_item.icons.clear();
  shortcut_item.icons.push_back(icon);

  manifest.shortcuts.push_back(shortcut_item);

  UpdateWebAppInfoFromManifest(manifest, kAppManifestUrl, &web_app_info);
  EXPECT_EQ(kAppTestTitle, web_app_info.title);
  EXPECT_EQ(DisplayMode::kMinimalUi, web_app_info.display_mode);
  // Sanity check that original copy was not changed.
  EXPECT_EQ(0u, web_app_info_original.shortcuts_menu_item_infos.size());

  // We currently duplicate the app icons with multiple Purposes.
  EXPECT_EQ(5u, web_app_info.manifest_icons.size());
  EXPECT_EQ(kAppIcon2, web_app_info.manifest_icons[0].url);
  EXPECT_EQ(kAppIcon3, web_app_info.manifest_icons[1].url);
  EXPECT_EQ(kAppIcon2, web_app_info.manifest_icons[2].url);
  EXPECT_EQ(kAppIcon3, web_app_info.manifest_icons[3].url);
  EXPECT_EQ(kAppIcon3, web_app_info.manifest_icons[4].url);

  EXPECT_EQ(2u, web_app_info.shortcuts_menu_item_infos.size());
  EXPECT_EQ(1u, web_app_info.shortcuts_menu_item_infos[0]
                    .GetShortcutIconInfosForPurpose(IconPurpose::ANY)
                    .size());
  WebAppShortcutsMenuItemInfo::Icon web_app_shortcut_icon =
      web_app_info.shortcuts_menu_item_infos[0].GetShortcutIconInfosForPurpose(
          IconPurpose::ANY)[0];
  EXPECT_EQ(kIconUrl2, web_app_shortcut_icon.url);

  EXPECT_EQ(0u, web_app_info.shortcuts_menu_item_infos[1]
                    .GetShortcutIconInfosForPurpose(IconPurpose::ANY)
                    .size());
  EXPECT_EQ(1u, web_app_info.shortcuts_menu_item_infos[1]
                    .GetShortcutIconInfosForPurpose(IconPurpose::MONOCHROME)
                    .size());
  EXPECT_EQ(1u, web_app_info.shortcuts_menu_item_infos[1]
                    .GetShortcutIconInfosForPurpose(IconPurpose::MASKABLE)
                    .size());
  web_app_shortcut_icon =
      web_app_info.shortcuts_menu_item_infos[1].GetShortcutIconInfosForPurpose(
          IconPurpose::MONOCHROME)[0];
  EXPECT_EQ(kIconUrl3, web_app_shortcut_icon.url);

  // Check file handlers were updated.
  ASSERT_EQ(1u, web_app_info.file_handlers.size());
  auto file_handler = web_app_info.file_handlers[0];
  ASSERT_EQ(1u, file_handler.accept.size());
  EXPECT_EQ(file_handler.accept[0].mime_type, "image/png");
  EXPECT_EQ(manifest.file_handlers[0]->action, file_handler.action);
  EXPECT_TRUE(file_handler.accept[0].file_extensions.contains(".png"));

  // Check protocol handlers were updated.
  EXPECT_EQ(1u, web_app_info.protocol_handlers.size());
  auto protocol_handler = web_app_info.protocol_handlers[0];
  EXPECT_EQ(protocol_handler.protocol, "mailto");
  EXPECT_EQ(protocol_handler.url, GURL("http://example.com/handle=%s"));

  // Check URL handlers were updated.
  EXPECT_EQ(1u, web_app_info.url_handlers.size());
  auto url_handler = web_app_info.url_handlers[0];
  EXPECT_EQ(url_handler.origin,
            url::Origin::Create(GURL("https://url_handlers_origin.com/")));
  EXPECT_TRUE(url_handler.has_origin_wildcard);
}

// Tests that we limit the number of icons declared by a site.
TEST(WebAppInstallUtils, UpdateWebAppInfoFromManifestTooManyIcons) {
  blink::mojom::Manifest manifest;
  for (int i = 0; i < 50; ++i) {
    blink::Manifest::ImageResource icon;
    icon.src = GURL("fav1.png");
    icon.purpose.push_back(Purpose::ANY);
    icon.sizes.emplace_back(i, i);
    manifest.icons.push_back(std::move(icon));
  }
  WebAppInstallInfo web_app_info;

  UpdateWebAppInfoFromManifest(
      manifest, GURL("http://www.chromium.org/manifest.json"), &web_app_info);
  EXPECT_EQ(20U, web_app_info.manifest_icons.size());
}

// Tests that we limit the number of shortcut icons declared by a site.
TEST(WebAppInstallUtils, UpdateWebAppInfoFromManifestTooManyShortcutIcons) {
  blink::mojom::Manifest manifest;
  for (unsigned int i = 0; i < kNumTestIcons; ++i) {
    blink::Manifest::ShortcutItem shortcut_item;
    shortcut_item.name = kShortcutItemTestName + base::NumberToString16(i);
    shortcut_item.url = GURL("http://www.chromium.org/shortcuts/action");

    blink::Manifest::ImageResource icon;
    icon.src = GURL("http://www.chromium.org/shortcuts/icon1.png");
    icon.sizes.emplace_back(i, i);
    icon.purpose.emplace_back(IconPurpose::ANY);
    shortcut_item.icons.push_back(std::move(icon));

    manifest.shortcuts.push_back(shortcut_item);
  }
  WebAppInstallInfo web_app_info;
  UpdateWebAppInfoFromManifest(
      manifest, GURL("http://www.chromium.org/manifest.json"), &web_app_info);

  std::vector<WebAppShortcutsMenuItemInfo::Icon> all_icons;
  for (const auto& shortcut : web_app_info.shortcuts_menu_item_infos) {
    for (const auto& icon_info :
         shortcut.GetShortcutIconInfosForPurpose(IconPurpose::ANY)) {
      all_icons.push_back(icon_info);
    }
  }
  ASSERT_GT(kNumTestIcons, all_icons.size());
  EXPECT_EQ(20U, all_icons.size());
}

// Tests that we limit the size of icons declared by a site.
TEST(WebAppInstallUtils, UpdateWebAppInfoFromManifestIconsTooLarge) {
  blink::mojom::Manifest manifest;
  for (int i = 1; i <= 20; ++i) {
    blink::Manifest::ImageResource icon;
    icon.src = GURL("fav1.png");
    icon.purpose.push_back(Purpose::ANY);
    const int size = i * 100;
    icon.sizes.emplace_back(size, size);
    manifest.icons.push_back(std::move(icon));
  }

  WebAppInstallInfo web_app_info;
  UpdateWebAppInfoFromManifest(
      manifest, GURL("http://www.chromium.org/manifest.json"), &web_app_info);

  EXPECT_EQ(10U, web_app_info.manifest_icons.size());
  for (const apps::IconInfo& icon : web_app_info.manifest_icons) {
    EXPECT_LE(icon.square_size_px, 1024);
  }
}

// Tests that we limit the size of shortcut icons declared by a site.
TEST(WebAppInstallUtils, UpdateWebAppInfoFromManifestShortcutIconsTooLarge) {
  blink::mojom::Manifest manifest;
  for (int i = 1; i <= 20; ++i) {
    blink::Manifest::ShortcutItem shortcut_item;
    shortcut_item.name = kShortcutItemTestName + base::NumberToString16(i);
    shortcut_item.url = GURL("http://www.chromium.org/shortcuts/action");

    blink::Manifest::ImageResource icon;
    icon.src = GURL("http://www.chromium.org/shortcuts/icon1.png");
    icon.purpose.push_back(Purpose::ANY);
    const int size = i * 100;
    icon.sizes.emplace_back(size, size);
    shortcut_item.icons.push_back(std::move(icon));

    manifest.shortcuts.push_back(shortcut_item);
  }
  WebAppInstallInfo web_app_info;
  UpdateWebAppInfoFromManifest(
      manifest, GURL("http://www.chromium.org/manifest.json"), &web_app_info);

  std::vector<WebAppShortcutsMenuItemInfo::Icon> all_icons;
  for (const auto& shortcut : web_app_info.shortcuts_menu_item_infos) {
    for (const auto& icon_info :
         shortcut.GetShortcutIconInfosForPurpose(IconPurpose::ANY)) {
      all_icons.push_back(icon_info);
    }
  }
  EXPECT_EQ(10U, all_icons.size());
}

// Tests that SkBitmaps associated with shortcut item icons are populated in
// their own map in web_app_info.
TEST(WebAppInstallUtils, PopulateShortcutItemIcons) {
  WebAppInstallInfo web_app_info;
  WebAppShortcutsMenuItemInfo::Icon icon;

  const GURL kIconUrl1("http://www.chromium.org/shortcuts/icon1.png");
  {
    WebAppShortcutsMenuItemInfo shortcut_item;
    std::vector<WebAppShortcutsMenuItemInfo::Icon> shortcut_manifest_icons;
    shortcut_item.name = std::u16string(kShortcutItemTestName) + u"1";
    shortcut_item.url = GURL("http://www.chromium.org/shortcuts/action");
    icon.url = kIconUrl1;
    icon.square_size_px = kIconSize;
    shortcut_manifest_icons.push_back(icon);
    shortcut_item.SetShortcutIconInfosForPurpose(
        IconPurpose::ANY, std::move(shortcut_manifest_icons));
    web_app_info.shortcuts_menu_item_infos.push_back(std::move(shortcut_item));
  }

  const GURL kIconUrl2("http://www.chromium.org/shortcuts/icon2.png");
  {
    WebAppShortcutsMenuItemInfo shortcut_item;
    std::vector<WebAppShortcutsMenuItemInfo::Icon> shortcut_manifest_icons;
    shortcut_item.name = std::u16string(kShortcutItemTestName) + u"2";
    icon.url = kIconUrl1;
    icon.square_size_px = kIconSize;
    shortcut_manifest_icons.push_back(icon);
    icon.url = kIconUrl2;
    icon.square_size_px = 2 * kIconSize;
    shortcut_manifest_icons.push_back(icon);
    shortcut_item.SetShortcutIconInfosForPurpose(
        IconPurpose::ANY, std::move(shortcut_manifest_icons));
    web_app_info.shortcuts_menu_item_infos.push_back(std::move(shortcut_item));
  }

  {
    IconsMap icons_map;
    std::vector<SkBitmap> bmp1 = {CreateSquareIcon(32, SK_ColorWHITE)};
    std::vector<SkBitmap> bmp2 = {CreateSquareIcon(32, SK_ColorBLUE)};
    std::vector<SkBitmap> bmp3 = {CreateSquareIcon(32, SK_ColorRED)};
    icons_map.emplace(kIconUrl1, bmp1);
    icons_map.emplace(kIconUrl2, bmp2);
    icons_map.emplace(GURL("http://www.chromium.org/shortcuts/icon3.png"),
                      bmp3);
    PopulateOtherIcons(&web_app_info, icons_map);
  }

  // Ensure that reused shortcut icons are processed correctly.
  EXPECT_EQ(1U, web_app_info.shortcuts_menu_icon_bitmaps[0].any.size());
  EXPECT_EQ(0U, web_app_info.shortcuts_menu_icon_bitmaps[0].maskable.size());
  EXPECT_EQ(2U, web_app_info.shortcuts_menu_icon_bitmaps[1].any.size());
  EXPECT_EQ(0U, web_app_info.shortcuts_menu_icon_bitmaps[1].maskable.size());
}

// Tests that when PopulateOtherItemIcons is called with no shortcut icon
// urls specified, no data is written to shortcuts_menu_item_infos.
TEST(WebAppInstallUtils, PopulateShortcutItemIconsNoShortcutIcons) {
  WebAppInstallInfo web_app_info;
  IconsMap icons_map;
  std::vector<SkBitmap> bmp1 = {CreateSquareIcon(32, SK_ColorWHITE)};
  std::vector<SkBitmap> bmp2 = {CreateSquareIcon(32, SK_ColorBLUE)};
  std::vector<SkBitmap> bmp3 = {CreateSquareIcon(32, SK_ColorRED)};
  icons_map.emplace(GURL("http://www.chromium.org/shortcuts/icon1.png"), bmp1);
  icons_map.emplace(GURL("http://www.chromium.org/shortcuts/icon2.png"), bmp2);
  icons_map.emplace(GURL("http://www.chromium.org/shortcuts/icon3.png"), bmp3);

  PopulateOtherIcons(&web_app_info, icons_map);

  EXPECT_EQ(0U, web_app_info.shortcuts_menu_item_infos.size());
}

// Tests that when PopulateProductIcons is called with maskable
// icons available, web_app_info.icon_bitmaps_{any,maskable} are correctly
// populated.
TEST(WebAppInstallUtils, PopulateProductIcons_MaskableIcons) {
  // Construct |icons_map| to pass to PopulateProductIcons().
  IconsMap icons_map;
  const GURL kIconUrl1("http://www.chromium.org/shortcuts/icon1.png");
  std::vector<SkBitmap> bmp1 = {CreateSquareIcon(32, SK_ColorWHITE)};
  icons_map.emplace(kIconUrl1, bmp1);
  const GURL kIconUrl2("http://www.chromium.org/shortcuts/icon2.png");
  std::vector<SkBitmap> bmp2 = {CreateSquareIcon(64, SK_ColorBLUE)};
  icons_map.emplace(kIconUrl2, bmp2);

  // Construct |web_app_info| to pass icon infos.
  WebAppInstallInfo web_app_info;
  web_app_info.title = u"App Name";
  apps::IconInfo info;
  // Icon at URL 1 has both kAny and kMaskable purpose.
  info.url = kIconUrl1;
  info.purpose = apps::IconInfo::Purpose::kAny;
  web_app_info.manifest_icons.push_back(info);
  info.purpose = apps::IconInfo::Purpose::kMaskable;
  web_app_info.manifest_icons.push_back(info);
  // Icon at URL 2 has kMaskable purpose only.
  info.url = kIconUrl2;
  info.purpose = apps::IconInfo::Purpose::kMaskable;
  web_app_info.manifest_icons.push_back(info);

  PopulateProductIcons(&web_app_info, &icons_map);

  EXPECT_EQ(SizesToGenerate().size(), web_app_info.icon_bitmaps.any.size());
  // Expect only icon at URL 1 to be used and resized as.
  for (const auto& icon_bitmap : web_app_info.icon_bitmaps.any) {
    EXPECT_EQ(SK_ColorWHITE, icon_bitmap.second.getColor(0, 0));
  }
  EXPECT_EQ(2u, web_app_info.icon_bitmaps.maskable.size());
}

// Tests that when PopulateProductIcons is called with maskable
// icons only, web_app_info.icon_bitmaps_any is correctly populated.
TEST(WebAppInstallUtils, PopulateProductIcons_MaskableIconsOnly) {
  // Construct |icons_map| to pass to PopulateProductIcons().
  IconsMap icons_map;
  const GURL kIconUrl1("http://www.chromium.org/shortcuts/icon1.png");
  std::vector<SkBitmap> bmp1 = {CreateSquareIcon(32, SK_ColorWHITE)};
  icons_map.emplace(kIconUrl1, bmp1);

  // Construct |web_app_info| to pass icon infos.
  WebAppInstallInfo web_app_info;
  web_app_info.title = u"App Name";
  apps::IconInfo info;
  info.url = kIconUrl1;
  info.purpose = apps::IconInfo::Purpose::kMaskable;
  web_app_info.manifest_icons.push_back(info);

  PopulateProductIcons(&web_app_info, &icons_map);

  // Expect to fall back to using icon from icons_map.
  EXPECT_EQ(SizesToGenerate().size(), web_app_info.icon_bitmaps.any.size());
  for (const auto& icon_bitmap : web_app_info.icon_bitmaps.any) {
    EXPECT_EQ(SK_ColorWHITE, icon_bitmap.second.getColor(0, 0));
  }
}

TEST(WebAppInstallUtils, UpdateWebAppInfoFromManifest_InvalidManifestUrl) {
  WebAppInstallInfo web_app_info;
  blink::mojom::Manifest manifest;

  UpdateWebAppInfoFromManifest(manifest, GURL("foo"), &web_app_info);
  EXPECT_TRUE(web_app_info.manifest_url.is_empty());
}

// Tests that when PopulateProductIcons is called with no
// app icon or shortcut icon data in web_app_info, and kDesktopPWAShortcutsMenu
// feature enabled, web_app_info.icon_bitmaps_any is correctly populated.
TEST(WebAppInstallUtils, PopulateProductIconsNoWebAppIconData_WithShortcuts) {
  WebAppInstallInfo web_app_info;
  web_app_info.title = u"App Name";

  IconsMap icons_map;
  std::vector<SkBitmap> bmp1 = {CreateSquareIcon(32, SK_ColorWHITE)};
  icons_map.emplace(GURL("http://www.chromium.org/shortcuts/icon1.png"), bmp1);
  PopulateProductIcons(&web_app_info, &icons_map);

  // Expect to fall back to using icon from icons_map.
  EXPECT_EQ(SizesToGenerate().size(), web_app_info.icon_bitmaps.any.size());
  for (const auto& icon_bitmap : web_app_info.icon_bitmaps.any) {
    EXPECT_EQ(SK_ColorWHITE, icon_bitmap.second.getColor(0, 0));
  }
}

TEST(WebAppInstallUtils, PopulateProductIcons_IsGeneratedIcon) {
  {
    WebAppInstallInfo web_app_info;
    web_app_info.title = u"App Name";

    IconsMap icons_map;
    PopulateProductIcons(&web_app_info, &icons_map);

    EXPECT_TRUE(web_app_info.is_generated_icon);

    EXPECT_TRUE(ContainsOneIconOfEachSize(web_app_info.icon_bitmaps.any));
  }
  {
    WebAppInstallInfo web_app_info;
    web_app_info.title = u"App Name";

    IconsMap icons_map;
    AddIconToIconsMap(GURL("http://www.example.org/icon32.png"), icon_size::k32,
                      SK_ColorCYAN, &icons_map);

    // Does upsizing of the smallest icon.
    PopulateProductIcons(&web_app_info, &icons_map);

    EXPECT_FALSE(web_app_info.is_generated_icon);

    EXPECT_TRUE(ContainsOneIconOfEachSize(web_app_info.icon_bitmaps.any));
    for (const auto& bitmap_any : web_app_info.icon_bitmaps.any)
      EXPECT_EQ(SK_ColorCYAN, bitmap_any.second.getColor(0, 0));
  }
  {
    WebAppInstallInfo web_app_info;
    web_app_info.title = u"App Name";

    IconsMap icons_map;
    AddIconToIconsMap(GURL("http://www.example.org/icon512.png"),
                      icon_size::k512, SK_ColorMAGENTA, &icons_map);

    // Does downsizing of the biggest icon which is not in `SizesToGenerate()`.
    PopulateProductIcons(&web_app_info, &icons_map);

    EXPECT_FALSE(web_app_info.is_generated_icon);

    EXPECT_TRUE(ContainsOneIconOfEachSize(web_app_info.icon_bitmaps.any));
    for (const auto& bitmap_any : web_app_info.icon_bitmaps.any)
      EXPECT_EQ(SK_ColorMAGENTA, bitmap_any.second.getColor(0, 0));
  }
}

TEST(WebAppInstallUtils, UpdateWebAppInfoFromManifest_Translations) {
  blink::mojom::Manifest manifest;
  WebAppInstallInfo web_app_info;

  {
    blink::Manifest::TranslationItem item;
    item.name = "name 1";
    item.short_name = "short name 1";
    item.description = "description 1";

    manifest.translations[u"language 1"] = std::move(item);
  }
  {
    blink::Manifest::TranslationItem item;
    item.short_name = "short name 2";
    item.description = "description 2";

    manifest.translations[u"language 2"] = std::move(item);
  }
  {
    blink::Manifest::TranslationItem item;
    item.name = "name 3";

    manifest.translations[u"language 3"] = std::move(item);
  }

  const GURL kAppManifestUrl("http://www.chromium.org/manifest.json");
  UpdateWebAppInfoFromManifest(manifest, kAppManifestUrl, &web_app_info);

  EXPECT_EQ(3u, web_app_info.translations.size());
  EXPECT_EQ(web_app_info.translations[u"language 1"].name, "name 1");
  EXPECT_EQ(web_app_info.translations[u"language 1"].short_name,
            "short name 1");
  EXPECT_EQ(web_app_info.translations[u"language 1"].description,
            "description 1");

  EXPECT_FALSE(web_app_info.translations[u"language 2"].name);
  EXPECT_EQ(web_app_info.translations[u"language 2"].short_name,
            "short name 2");
  EXPECT_EQ(web_app_info.translations[u"language 2"].description,
            "description 2");

  EXPECT_EQ(web_app_info.translations[u"language 3"].name, "name 3");
  EXPECT_FALSE(web_app_info.translations[u"language 3"].short_name);
  EXPECT_FALSE(web_app_info.translations[u"language 3"].description);
}

class FileHandlersFromManifestTest : public ::testing::TestWithParam<bool> {
 public:
  FileHandlersFromManifestTest() {
    feature_list_.InitWithFeatures({blink::features::kFileHandlingIcons}, {});
    WebAppFileHandlerManager::SetIconsSupportedByOsForTesting(GetParam());
  }

  ~FileHandlersFromManifestTest() override = default;

 protected:
  static std::vector<blink::mojom::ManifestFileHandlerPtr>
  CreateManifestFileHandlers(unsigned count) {
    std::vector<blink::mojom::ManifestFileHandlerPtr> manifest_file_handlers;
    for (unsigned i = 0; i < count; ++i) {
      auto file_handler = blink::mojom::ManifestFileHandler::New();
      file_handler->action = MakeActionUrl(i);
      file_handler->name = base::UTF8ToUTF16(base::StringPrintf("n%u", i));
      file_handler->accept[base::UTF8ToUTF16(MakeMimeType(i))] = {
          base::UTF8ToUTF16(MakeExtension(i))};

      blink::Manifest::ImageResource icon;
      icon.src = MakeImageUrl(i);
      icon.sizes = {{16, 16}, {32, 32}, {64, 64}};
      icon.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};
      file_handler->icons.push_back(std::move(icon));

      blink::Manifest::ImageResource icon2;
      icon2.src = MakeImageUrlForSecondImage(i);
      icon2.sizes = {{16, 16}};
      icon2.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY,
                       blink::mojom::ManifestImageResource_Purpose::MASKABLE};
      file_handler->icons.push_back(std::move(icon2));

      manifest_file_handlers.push_back(std::move(file_handler));
    }
    return manifest_file_handlers;
  }

  static GURL MakeActionUrl(unsigned index) {
    return GetStartUrl().Resolve(base::StringPrintf("a%u", index));
  }

  static GURL MakeImageUrl(unsigned index) {
    return GetStartUrl().Resolve(base::StringPrintf("image%u.png", index));
  }

  static GURL MakeImageUrlForSecondImage(unsigned index) {
    return GetStartUrl().Resolve(base::StringPrintf("image%u-2.png", index));
  }

  static std::string MakeMimeType(unsigned index) {
    return base::StringPrintf("application/x-%u", index);
  }

  static std::string MakeExtension(unsigned index) {
    return base::StringPrintf(".e%u", index);
  }

  static GURL GetStartUrl() {
    return GURL("https://www.example.com/index.html");
  }

  base::test::ScopedFeatureList feature_list_;
};

TEST_P(FileHandlersFromManifestTest, Basic) {
  std::vector<blink::mojom::ManifestFileHandlerPtr> manifest_file_handlers =
      CreateManifestFileHandlers(6);

  apps::FileHandlers file_handlers =
      CreateFileHandlersFromManifest(manifest_file_handlers, GetStartUrl());
  ASSERT_EQ(file_handlers.size(), 6U);
  for (unsigned i = 0; i < 6U; ++i) {
    EXPECT_EQ(file_handlers[i].action, MakeActionUrl(i));
    ASSERT_EQ(file_handlers[i].accept.size(), 1U);
    EXPECT_EQ(file_handlers[i].accept[0].mime_type, MakeMimeType(i));
    EXPECT_EQ(file_handlers[i].accept[0].file_extensions.size(), 1U);
    EXPECT_EQ(*file_handlers[i].accept[0].file_extensions.begin(),
              MakeExtension(i));

    if (WebAppFileHandlerManager::IconsEnabled()) {
      ASSERT_EQ(file_handlers[i].downloaded_icons.size(), 3U);

      // The manifest-specified `sizes` are ignored.
      EXPECT_FALSE(file_handlers[i].downloaded_icons[0].square_size_px);
      EXPECT_EQ(MakeImageUrl(i), file_handlers[i].downloaded_icons[0].url);
      EXPECT_EQ(apps::IconInfo::Purpose::kAny,
                file_handlers[i].downloaded_icons[0].purpose);

      EXPECT_FALSE(file_handlers[i].downloaded_icons[1].square_size_px);
      EXPECT_EQ(MakeImageUrlForSecondImage(i),
                file_handlers[i].downloaded_icons[1].url);
      EXPECT_EQ(apps::IconInfo::Purpose::kAny,
                file_handlers[i].downloaded_icons[1].purpose);

      EXPECT_FALSE(file_handlers[i].downloaded_icons[2].square_size_px);
      EXPECT_EQ(MakeImageUrlForSecondImage(i),
                file_handlers[i].downloaded_icons[2].url);
      EXPECT_EQ(apps::IconInfo::Purpose::kMaskable,
                file_handlers[i].downloaded_icons[2].purpose);
    } else {
      EXPECT_TRUE(file_handlers[i].downloaded_icons.empty());
    }
  }
}

TEST_P(FileHandlersFromManifestTest, PopulateFileHandlerIcons) {
  if (!WebAppFileHandlerManager::IconsEnabled())
    return;

  std::vector<blink::mojom::ManifestFileHandlerPtr> manifest_file_handlers =
      CreateManifestFileHandlers(1);
  WebAppInstallInfo web_app_info;
  web_app_info.file_handlers =
      CreateFileHandlersFromManifest(manifest_file_handlers, GetStartUrl());

  const GURL first_image_url = MakeImageUrl(0);
  const GURL second_image_url = MakeImageUrlForSecondImage(0);
  IconsMap icons_map;
  // The first URL returns two valid bitmaps and one invalid (non-square), which
  // should be ignored.
  std::vector<SkBitmap> bmps1 = {CreateSquareIcon(17, SK_ColorWHITE),
                                 CreateSquareIcon(29, SK_ColorBLUE),
                                 gfx::test::CreateBitmap(16, 15)};
  icons_map.emplace(first_image_url, bmps1);
  std::vector<SkBitmap> bmps2 = {CreateSquareIcon(79, SK_ColorRED),
                                 CreateSquareIcon(134, SK_ColorRED)};
  icons_map.emplace(second_image_url, bmps2);
  PopulateOtherIcons(&web_app_info, icons_map);

  // Make sure bitmaps are copied from `icons_map` into `web_app_info`.
  // Images downloaded from two distinct URLs.
  ASSERT_EQ(2U, web_app_info.other_icon_bitmaps.size());
  // First URL correlates to two bitmaps.
  ASSERT_EQ(2U, web_app_info.other_icon_bitmaps[first_image_url].size());
  EXPECT_TRUE(
      gfx::BitmapsAreEqual(web_app_info.other_icon_bitmaps[first_image_url][0],
                           icons_map[first_image_url][0]));
  EXPECT_TRUE(
      gfx::BitmapsAreEqual(web_app_info.other_icon_bitmaps[first_image_url][1],
                           icons_map[first_image_url][1]));
  // Second URL correlates to two more bitmaps.
  ASSERT_EQ(2U, web_app_info.other_icon_bitmaps[second_image_url].size());
  EXPECT_TRUE(
      gfx::BitmapsAreEqual(web_app_info.other_icon_bitmaps[second_image_url][0],
                           icons_map[second_image_url][0]));
  EXPECT_TRUE(
      gfx::BitmapsAreEqual(web_app_info.other_icon_bitmaps[second_image_url][1],
                           icons_map[second_image_url][1]));

  // We end up with one file handler with 6 icon infos. The second URL produces
  // 4 IconInfos because it has two bitmaps and two purposes: 2 x 2 = 4.
  ASSERT_EQ(1U, web_app_info.file_handlers.size());

  // The metadata we expect to be saved after icons are finished downloading and
  // processing. Note that the icon sizes saved to `apps::FileHandler::icons`
  // match downloaded sizes, not those specified in the manifest.
  struct {
    GURL expected_url;
    apps::IconInfo::SquareSizePx expected_size;
    apps::IconInfo::Purpose expected_purpose;
  } expectations[] = {
      {first_image_url, 17, apps::IconInfo::Purpose::kAny},
      {first_image_url, 29, apps::IconInfo::Purpose::kAny},
      {second_image_url, 79, apps::IconInfo::Purpose::kAny},
      {second_image_url, 134, apps::IconInfo::Purpose::kAny},
      {second_image_url, 79, apps::IconInfo::Purpose::kMaskable},
      {second_image_url, 134, apps::IconInfo::Purpose::kMaskable},
  };

  const size_t num_expectations =
      sizeof(expectations) / sizeof(expectations[0]);
  ASSERT_EQ(num_expectations,
            web_app_info.file_handlers[0].downloaded_icons.size());

  for (size_t i = 0; i < num_expectations; ++i) {
    const auto& icon = web_app_info.file_handlers[0].downloaded_icons[i];
    EXPECT_EQ(expectations[i].expected_url, icon.url);
    EXPECT_EQ(expectations[i].expected_size, icon.square_size_px);
    EXPECT_EQ(expectations[i].expected_purpose, icon.purpose);
  }
}

INSTANTIATE_TEST_SUITE_P(, FileHandlersFromManifestTest, testing::Bool());

#if BUILDFLAG(IS_WIN)
class RegisterOsSettingsTest : public testing::Test {
 public:
  RegisterOsSettingsTest() {
    feature_list_.InitWithFeatures(
        {features::kEnableWebAppUninstallFromOsSettings}, {});
  }
  ~RegisterOsSettingsTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment browser_task_environment_;
};

TEST_F(RegisterOsSettingsTest, MaybeRegisterOsUninstall) {
  // MaybeRegisterOsUninstall
  // Scenario 1.
  // web app sources: kDefault, kPolicy
  // removed source: kPolicy
  // check web_app.CanUserUninstallWebApp is false
  // check RegisterWebAppOsUninstallation is called
  const AppId app_id = "test";
  testing::StrictMock<MockOsIntegrationManager> manager;
  // InstallOsHooks from MaybeRegisterOsUninstall
  // sets only kUninstallationViaOsSettings that will async call from
  // InstallOsHooks. Test ends before async is called so we test against
  // InstallOsHooks.
  EXPECT_CALL(manager, MacAppShimOnAppInstalledForProfile(app_id)).Times(1);
  EXPECT_CALL(manager, RegisterWebAppOsUninstallation(app_id, testing::_))
      .Times(1);

  // Scenario 1.
  auto web_app = std::make_unique<WebApp>(app_id);
  web_app->AddSource(Source::kDefault);
  web_app->AddSource(Source::kPolicy);
  EXPECT_FALSE(web_app->CanUserUninstallWebApp());

  base::RunLoop run_loop;
  MaybeRegisterOsUninstall(
      web_app.get(), Source::kPolicy, manager,
      base::BindLambdaForTesting(
          [&](OsHooksErrors os_hooks_errors) { run_loop.Quit(); }));
  run_loop.Run();
}

TEST_F(RegisterOsSettingsTest, MaybeRegisterOsSettings_NoRegistration) {
  // MaybeRegisterOsUninstall
  // Scenario 2.
  // web app sources: kSync, kPolicy
  // removed source: kSync
  // check web_app.CanUserUninstallWebApp is false
  // check RegisterWebAppOsUninstallation is not called

  // Scenario 3.
  // web app sources: kDefault, kSync, kWewbAppStore
  // removed source: kSync
  // check web_app.CanUserUninstallWebApp is true
  // check RegisterWebAppOsUninstallation is not called
  const AppId app_id = "test";
  testing::StrictMock<MockOsIntegrationManager> manager;
  // InstallOsHooks from MaybeRegisterOsUninstall
  // sets only kUninstallationViaOsSettings that will async call from
  // InstallOsHooks. Test ends before async is called so we test against
  // InstallOsHooks.
  EXPECT_CALL(manager, RegisterWebAppOsUninstallation(app_id, testing::_))
      .Times(0);

  // Scenario 2.
  auto web_app = std::make_unique<WebApp>(app_id);
  web_app->AddSource(Source::kSync);
  web_app->AddSource(Source::kPolicy);
  EXPECT_FALSE(web_app->CanUserUninstallWebApp());
  MaybeRegisterOsUninstall(web_app.get(), Source::kSync, manager,
                           base::DoNothing());

  // Scenario 3.
  auto web_app2 = std::make_unique<WebApp>(app_id);
  web_app2->AddSource(Source::kDefault);
  web_app2->AddSource(Source::kSync);
  web_app2->AddSource(Source::kWebAppStore);
  EXPECT_TRUE(web_app2->CanUserUninstallWebApp());
  MaybeRegisterOsUninstall(web_app2.get(), Source::kDefault, manager,
                           base::DoNothing());
}

TEST_F(RegisterOsSettingsTest, MaybeUnregisterOsUninstall) {
  // MaybeUnregisterOsUninstall
  // Scenario 1.
  // web app sources: kDefault
  // added source: kPolicy
  // check web_app.CanUserUninstallWebApp is false
  // check UnregisterWebAppOsUninstallation is called
  const AppId app_id = "test";
  testing::StrictMock<MockOsIntegrationManager> manager;
  // InstallOsHooks from MaybeRegisterOsUninstall
  // sets only kUninstallationViaOsSettings that will async call from
  // InstallOsHooks. Test ends before async is called so we test against
  // InstallOsHooks.
  EXPECT_CALL(manager, UnregisterWebAppOsUninstallation(app_id)).Times(1);

  // Scenario 1.
  auto web_app = std::make_unique<WebApp>(app_id);
  web_app->AddSource(Source::kDefault);
  EXPECT_TRUE(web_app->CanUserUninstallWebApp());
  MaybeUnregisterOsUninstall(web_app.get(), Source::kPolicy, manager);
}

TEST_F(RegisterOsSettingsTest, MaybeUnregisterOsSettings_NoUnregistration) {
  // MaybeUnregisterOsUninstall
  // Scenario 2.
  // web app sources: kSync, kPolicy
  // added source: kSync
  // check web_app.CanUserUninstallWebApp is false
  // check UnregisterWebAppOsUninstallation is not called

  // Scenario 3.
  // web app sources: kSync
  // added source: kSync
  // check web_app.CanUserUninstallWebApp is true
  // check UnregisterWebAppOsUninstallation is not called
  const AppId app_id = "test";
  testing::StrictMock<MockOsIntegrationManager> manager;
  // InstallOsHooks from MaybeRegisterOsUninstall
  // sets only kUninstallationViaOsSettings that will async call from
  // InstallOsHooks. Test ends before async is called so we test against
  // InstallOsHooks.
  EXPECT_CALL(manager, UnregisterWebAppOsUninstallation(app_id)).Times(0);

  // Scenario 2.
  auto web_app = std::make_unique<WebApp>(app_id);
  web_app->AddSource(Source::kPolicy);
  EXPECT_FALSE(web_app->CanUserUninstallWebApp());
  MaybeUnregisterOsUninstall(web_app.get(), Source::kSync, manager);

  // Scenario 3.
  auto web_app2 = std::make_unique<WebApp>(app_id);
  web_app2->AddSource(Source::kSync);
  EXPECT_TRUE(web_app2->CanUserUninstallWebApp());
  MaybeUnregisterOsUninstall(web_app2.get(), Source::kDefault, manager);
}

#endif  // BUILDFLAG(IS_WIN)

}  // namespace web_app
