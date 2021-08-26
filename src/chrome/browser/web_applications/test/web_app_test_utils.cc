// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/web_app_test_utils.h"

#include <random>

#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "components/services/app_service/public/cpp/url_handler_info.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "url/gurl.h"

namespace web_app {
namespace test {

namespace {

class RandomHelper {
 public:
  explicit RandomHelper(const uint32_t seed)
      :  // Seed of 0 and 1 generate the same sequence, so skip 0.
        generator_(seed + 1),
        distribution_(0u, UINT32_MAX) {}

  uint32_t next_uint() { return distribution_(generator_); }

  // Return an unsigned int between 0 (inclusive) and bound (exclusive).
  uint32_t next_uint(uint32_t bound) { return next_uint() % bound; }

  bool next_bool() { return next_uint() & 1u; }

  template <typename T>
  T next_enum() {
    constexpr uint32_t min = static_cast<uint32_t>(T::kMinValue);
    constexpr uint32_t max = static_cast<uint32_t>(T::kMaxValue);
    static_assert(min <= max, "min cannot be greater than max");
    return static_cast<T>(min + next_uint(max - min));
  }

 private:
  std::default_random_engine generator_;
  std::uniform_int_distribution<uint32_t> distribution_;
};

apps::FileHandlers CreateRandomFileHandlers(uint32_t suffix) {
  apps::FileHandlers file_handlers;

  for (unsigned int i = 0; i < 5; ++i) {
    std::string suffix_str =
        base::NumberToString(suffix) + base::NumberToString(i);

    apps::FileHandler::AcceptEntry accept_entry1;
    accept_entry1.mime_type = "application/" + suffix_str + "+foo";
    accept_entry1.file_extensions.insert("." + suffix_str + "a");
    accept_entry1.file_extensions.insert("." + suffix_str + "b");

    apps::FileHandler::AcceptEntry accept_entry2;
    accept_entry2.mime_type = "application/" + suffix_str + "+bar";
    accept_entry2.file_extensions.insert("." + suffix_str + "a");
    accept_entry2.file_extensions.insert("." + suffix_str + "b");

    apps::FileHandler file_handler;
    file_handler.action = GURL("https://example.com/open-" + suffix_str);
    file_handler.accept.push_back(std::move(accept_entry1));
    file_handler.accept.push_back(std::move(accept_entry2));

    file_handlers.push_back(std::move(file_handler));
  }

  return file_handlers;
}

apps::ShareTarget CreateRandomShareTarget(uint32_t suffix) {
  apps::ShareTarget share_target;
  share_target.action =
      GURL("https://example.com/path/target/" + base::NumberToString(suffix));
  share_target.method = (suffix % 2 == 0) ? apps::ShareTarget::Method::kPost
                                          : apps::ShareTarget::Method::kGet;
  share_target.enctype = (suffix / 2 % 2 == 0)
                             ? apps::ShareTarget::Enctype::kMultipartFormData
                             : apps::ShareTarget::Enctype::kFormUrlEncoded;

  if (suffix % 3 != 0)
    share_target.params.title = "title" + base::NumberToString(suffix);
  if (suffix % 3 != 1)
    share_target.params.text = "text" + base::NumberToString(suffix);
  if (suffix % 3 != 2)
    share_target.params.url = "url" + base::NumberToString(suffix);

  for (uint32_t index = 0; index < suffix % 5; ++index) {
    apps::ShareTarget::Files files;
    files.name = "files" + base::NumberToString(index);
    files.accept.push_back(".extension" + base::NumberToString(index));
    files.accept.push_back("type/subtype" + base::NumberToString(index));
    share_target.params.files.push_back(files);
  }

  return share_target;
}

std::vector<apps::ProtocolHandlerInfo> CreateRandomProtocolHandlers(
    uint32_t suffix) {
  std::vector<apps::ProtocolHandlerInfo> protocol_handlers;

  for (unsigned int i = 0; i < 5; ++i) {
    std::string suffix_str =
        base::NumberToString(suffix) + base::NumberToString(i);

    apps::ProtocolHandlerInfo protocol_handler;
    protocol_handler.protocol = "web+test" + suffix_str;
    protocol_handler.url = GURL("https://example.com/").Resolve(suffix_str);

    protocol_handlers.push_back(std::move(protocol_handler));
  }

  return protocol_handlers;
}

std::vector<apps::UrlHandlerInfo> CreateRandomUrlHandlers(uint32_t suffix) {
  std::vector<apps::UrlHandlerInfo> url_handlers;

  for (unsigned int i = 0; i < 3; ++i) {
    std::string suffix_str =
        base::NumberToString(suffix) + base::NumberToString(i);

    apps::UrlHandlerInfo url_handler;
    url_handler.origin =
        url::Origin::Create(GURL("https://app-" + suffix_str + ".com/"));
    url_handler.has_origin_wildcard = true;
    url_handlers.push_back(std::move(url_handler));
  }

  return url_handlers;
}

std::vector<WebApplicationShortcutsMenuItemInfo>
CreateRandomShortcutsMenuItemInfos(const GURL& scope, RandomHelper& random) {
  const uint32_t suffix = random.next_uint();
  std::vector<WebApplicationShortcutsMenuItemInfo> shortcuts_menu_item_infos;
  for (int i = random.next_uint(4) + 1; i >= 0; --i) {
    std::string suffix_str =
        base::NumberToString(suffix) + base::NumberToString(i);
    WebApplicationShortcutsMenuItemInfo shortcut_info;
    shortcut_info.url = scope.Resolve("shortcut" + suffix_str);
    shortcut_info.name = base::UTF8ToUTF16("shortcut" + suffix_str);

    std::vector<WebApplicationShortcutsMenuItemInfo::Icon> shortcut_icons_any;
    std::vector<WebApplicationShortcutsMenuItemInfo::Icon>
        shortcut_icons_maskable;
    std::vector<WebApplicationShortcutsMenuItemInfo::Icon>
        shortcut_icons_monochrome;

    for (int j = random.next_uint(4) + 1; j >= 0; --j) {
      std::string icon_suffix_str = suffix_str + base::NumberToString(j);
      WebApplicationShortcutsMenuItemInfo::Icon shortcut_icon;
      shortcut_icon.url = scope.Resolve("/shortcuts/icon" + icon_suffix_str);
      // Within each shortcut_icons_*, square_size_px must be unique.
      shortcut_icon.square_size_px = (j * 10) + random.next_uint(10);
      int icon_purpose = random.next_uint(3);
      switch (icon_purpose) {
        case 0:
          shortcut_icons_any.push_back(std::move(shortcut_icon));
          break;
        case 1:
          shortcut_icons_maskable.push_back(std::move(shortcut_icon));
          break;
        case 2:
          shortcut_icons_monochrome.push_back(std::move(shortcut_icon));
          break;
      }
    }

    shortcut_info.SetShortcutIconInfosForPurpose(IconPurpose::ANY,
                                                 std::move(shortcut_icons_any));
    shortcut_info.SetShortcutIconInfosForPurpose(
        IconPurpose::MASKABLE, std::move(shortcut_icons_maskable));
    shortcut_info.SetShortcutIconInfosForPurpose(
        IconPurpose::MONOCHROME, std::move(shortcut_icons_monochrome));

    shortcuts_menu_item_infos.emplace_back(std::move(shortcut_info));
  }
  return shortcuts_menu_item_infos;
}

std::vector<IconSizes> CreateRandomDownloadedShortcutsMenuIconsSizes(
    RandomHelper& random) {
  std::vector<IconSizes> results;
  for (unsigned int i = 0; i < 3; ++i) {
    IconSizes result;
    std::vector<SquareSizePx> shortcuts_menu_icon_sizes_any;
    std::vector<SquareSizePx> shortcuts_menu_icon_sizes_maskable;
    std::vector<SquareSizePx> shortcuts_menu_icon_sizes_monochrome;
    for (unsigned int j = 0; j < i; ++j) {
      shortcuts_menu_icon_sizes_any.emplace_back(random.next_uint(256) + 1);
      shortcuts_menu_icon_sizes_maskable.emplace_back(random.next_uint(256) +
                                                      1);
      shortcuts_menu_icon_sizes_monochrome.emplace_back(random.next_uint(256) +
                                                        1);
    }
    result.SetSizesForPurpose(IconPurpose::ANY,
                              std::move(shortcuts_menu_icon_sizes_any));
    result.SetSizesForPurpose(IconPurpose::MASKABLE,
                              std::move(shortcuts_menu_icon_sizes_maskable));
    result.SetSizesForPurpose(IconPurpose::MONOCHROME,
                              std::move(shortcuts_menu_icon_sizes_monochrome));
    results.emplace_back(std::move(result));
  }
  return results;
}

}  // namespace

std::unique_ptr<WebApp> CreateMinimalWebApp() {
  const GURL app_url("https://example.com/path");
  const AppId app_id = GenerateAppId(/*manifest_id=*/absl::nullopt, app_url);

  auto web_app = std::make_unique<WebApp>(app_id);
  web_app->AddSource(Source::kSync);
  web_app->SetDisplayMode(DisplayMode::kStandalone);
  web_app->SetUserDisplayMode(DisplayMode::kStandalone);
  web_app->SetName("Name");
  web_app->SetStartUrl(app_url);

  return web_app;
}

std::unique_ptr<WebApp> CreateRandomWebApp(const GURL& base_url,
                                           const uint32_t seed) {
  RandomHelper random(seed);

  const std::string seed_str = base::NumberToString(seed);
  absl::optional<std::string> manifest_id;
  if (random.next_bool())
    manifest_id = "manifest_id_" + seed_str;
  const GURL scope = base_url.Resolve("scope" + seed_str + "/");
  const GURL start_url = scope.Resolve("start" + seed_str);
  const AppId app_id = GenerateAppId(manifest_id, start_url);

  const std::string name = "Name" + seed_str;
  const std::string description = "Description" + seed_str;
  const absl::optional<SkColor> theme_color = random.next_uint();
  const absl::optional<SkColor> background_color = random.next_uint();
  const absl::optional<SkColor> synced_theme_color = random.next_uint();
  auto app = std::make_unique<WebApp>(app_id);

  // Generate all possible permutations of field values in a random way:
  if (random.next_bool())
    app->AddSource(Source::kSystem);
  if (random.next_bool())
    app->AddSource(Source::kPolicy);
  if (random.next_bool())
    app->AddSource(Source::kWebAppStore);
  if (random.next_bool())
    app->AddSource(Source::kSync);
  if (random.next_bool())
    app->AddSource(Source::kDefault);
  // Must always be at least one source.
  if (!app->HasAnySources())
    app->AddSource(Source::kSync);

  app->SetName(name);
  app->SetDescription(description);
  app->SetManifestId(manifest_id);
  app->SetStartUrl(GURL(start_url));
  app->SetScope(GURL(scope));
  app->SetThemeColor(theme_color);
  app->SetBackgroundColor(background_color);
  app->SetIsLocallyInstalled(random.next_bool());
  app->SetIsFromSyncAndPendingInstallation(random.next_bool());
  app->SetUserDisplayMode(random.next_bool() ? DisplayMode::kBrowser
                                             : DisplayMode::kStandalone);

  const base::Time last_badging_time =
      base::Time::UnixEpoch() +
      base::TimeDelta::FromMilliseconds(random.next_uint());
  app->SetLastBadgingTime(last_badging_time);

  const base::Time last_launch_time =
      base::Time::UnixEpoch() +
      base::TimeDelta::FromMilliseconds(random.next_uint());
  app->SetLastLaunchTime(last_launch_time);

  const base::Time install_time =
      base::Time::UnixEpoch() +
      base::TimeDelta::FromMilliseconds(random.next_uint());
  app->SetInstallTime(install_time);

  const DisplayMode display_modes[4] = {
      DisplayMode::kBrowser, DisplayMode::kMinimalUi, DisplayMode::kStandalone,
      DisplayMode::kFullscreen};
  app->SetDisplayMode(display_modes[random.next_uint(4)]);

  // Add only unique display modes.
  std::set<DisplayMode> display_mode_override;
  int num_display_mode_override_tries = random.next_uint(5);
  for (int i = 0; i < num_display_mode_override_tries; i++)
    display_mode_override.insert(display_modes[random.next_uint(4)]);
  app->SetDisplayModeOverride(std::vector<DisplayMode>(
      display_mode_override.begin(), display_mode_override.end()));

  if (random.next_bool())
    app->SetLaunchQueryParams(base::NumberToString(random.next_uint()));

  app->SetRunOnOsLoginMode(random.next_enum<RunOnOsLoginMode>());

  const SquareSizePx size = 256;
  const int num_icons = random.next_uint(10);
  std::vector<WebApplicationIconInfo> icon_infos(num_icons);
  for (int i = 0; i < num_icons; i++) {
    WebApplicationIconInfo icon;
    icon.url =
        base_url.Resolve("/icon" + base::NumberToString(random.next_uint()));
    if (random.next_bool())
      icon.square_size_px = size;

    int purpose = random.next_uint(4);
    if (purpose == 0)
      icon.purpose = blink::mojom::ManifestImageResource_Purpose::ANY;
    if (purpose == 1)
      icon.purpose = blink::mojom::ManifestImageResource_Purpose::MASKABLE;
    if (purpose == 2)
      icon.purpose = blink::mojom::ManifestImageResource_Purpose::MONOCHROME;
    // if (purpose == 3), leave purpose unset. Should default to ANY.

    icon_infos[i] = icon;
  }
  app->SetIconInfos(icon_infos);
  if (random.next_bool())
    app->SetDownloadedIconSizes(IconPurpose::ANY, {size});
  if (random.next_bool())
    app->SetDownloadedIconSizes(IconPurpose::MASKABLE, {size});
  if (random.next_bool())
    app->SetDownloadedIconSizes(IconPurpose::MONOCHROME, {size});
  app->SetIsGeneratedIcon(random.next_bool());

  app->SetFileHandlers(CreateRandomFileHandlers(random.next_uint()));
  if (random.next_bool())
    app->SetShareTarget(CreateRandomShareTarget(random.next_uint()));
  app->SetProtocolHandlers(CreateRandomProtocolHandlers(random.next_uint()));
  app->SetUrlHandlers(CreateRandomUrlHandlers(random.next_uint()));
  if (random.next_bool()) {
    app->SetNoteTakingNewNoteUrl(
        scope.Resolve("new_note" + base::NumberToString(random.next_uint())));
  }
  app->SetCaptureLinks(random.next_enum<blink::mojom::CaptureLinks>());

  const int num_additional_search_terms = random.next_uint(8);
  std::vector<std::string> additional_search_terms(num_additional_search_terms);
  for (int i = 0; i < num_additional_search_terms; ++i) {
    additional_search_terms[i] =
        "Foo_" + seed_str + "_" + base::NumberToString(i);
  }
  app->SetAdditionalSearchTerms(std::move(additional_search_terms));

  app->SetShortcutsMenuItemInfos(
      CreateRandomShortcutsMenuItemInfos(scope, random));
  app->SetDownloadedShortcutsMenuIconsSizes(
      CreateRandomDownloadedShortcutsMenuIconsSizes(random));
  app->SetManifestUrl(base_url.Resolve("/manifest" + seed_str + ".json"));

  const int num_approved_launch_protocols = random.next_uint(8);
  std::vector<std::string> approved_launch_protocols(
      num_approved_launch_protocols);
  for (int i = 0; i < num_approved_launch_protocols; ++i) {
    approved_launch_protocols[i] =
        "web+test_" + seed_str + "_" + base::NumberToString(i);
  }
  app->SetApprovedLaunchProtocols(std::move(approved_launch_protocols));

  app->SetStorageIsolated(random.next_bool());

  app->SetFileHandlerPermissionBlocked(false);

  app->SetWindowControlsOverlayEnabled(false);

  WebApp::SyncFallbackData sync_fallback_data;
  sync_fallback_data.name = "Sync" + name;
  sync_fallback_data.theme_color = synced_theme_color;
  sync_fallback_data.scope = app->scope();
  sync_fallback_data.icon_infos = app->icon_infos();
  app->SetSyncFallbackData(std::move(sync_fallback_data));

  if (random.next_bool()) {
    LaunchHandler launch_handler;
    launch_handler.route_to = random.next_enum<LaunchHandler::RouteTo>();
    launch_handler.navigate_existing_client =
        random.next_enum<LaunchHandler::NavigateExistingClient>();
    app->SetLaunchHandler(launch_handler);
  }

  // `random` should not be used after the chromeos block if the result
  // is expected to be deterministic across cros and non-cros builds.
  if (IsChromeOsDataMandatory()) {
    auto chromeos_data = absl::make_optional<WebAppChromeOsData>();
    chromeos_data->show_in_launcher = random.next_bool();
    chromeos_data->show_in_search = random.next_bool();
    chromeos_data->show_in_management = random.next_bool();
    chromeos_data->is_disabled = random.next_bool();
    chromeos_data->oem_installed = random.next_bool();
    app->SetWebAppChromeOsData(std::move(chromeos_data));
  }

  return app;
}

void TestAcceptDialogCallback(
    content::WebContents* initiator_web_contents,
    std::unique_ptr<WebApplicationInfo> web_app_info,
    ForInstallableSite for_installable_site,
    WebAppInstallationAcceptanceCallback acceptance_callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(acceptance_callback), true /*accept*/,
                                std::move(web_app_info)));
}

void TestDeclineDialogCallback(
    content::WebContents* initiator_web_contents,
    std::unique_ptr<WebApplicationInfo> web_app_info,
    ForInstallableSite for_installable_site,
    WebAppInstallationAcceptanceCallback acceptance_callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(acceptance_callback),
                                false /*accept*/, std::move(web_app_info)));
}

}  // namespace test
}  // namespace web_app
