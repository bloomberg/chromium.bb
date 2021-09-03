// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_installation_utils.h"

#include <map>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/services/app_service/public/cpp/protocol_handler_info.h"
#include "components/services/app_service/public/cpp/share_target.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"

namespace web_app {

namespace {

const char kChromeScheme[] = "chrome";

std::vector<SquareSizePx> GetSquareSizePxs(
    const std::map<SquareSizePx, SkBitmap>& icon_bitmaps) {
  std::vector<SquareSizePx> sizes;
  sizes.reserve(icon_bitmaps.size());
  for (const std::pair<const SquareSizePx, SkBitmap>& item : icon_bitmaps)
    sizes.push_back(item.first);
  return sizes;
}

std::vector<IconSizes> GetDownloadedShortcutsMenuIconsSizes(
    const ShortcutsMenuIconBitmaps& shortcuts_menu_icon_bitmaps) {
  std::vector<IconSizes> shortcuts_menu_icons_sizes;
  shortcuts_menu_icons_sizes.reserve(shortcuts_menu_icon_bitmaps.size());
  for (const auto& shortcut_icon_bitmaps : shortcuts_menu_icon_bitmaps) {
    IconSizes icon_sizes;
    icon_sizes.SetSizesForPurpose(IconPurpose::ANY,
                                  GetSquareSizePxs(shortcut_icon_bitmaps.any));
    icon_sizes.SetSizesForPurpose(
        IconPurpose::MASKABLE,
        GetSquareSizePxs(shortcut_icon_bitmaps.maskable));
    icon_sizes.SetSizesForPurpose(
        IconPurpose::MONOCHROME,
        GetSquareSizePxs(shortcut_icon_bitmaps.monochrome));
    shortcuts_menu_icons_sizes.push_back(std::move(icon_sizes));
  }
  return shortcuts_menu_icons_sizes;
}

void SetWebAppFileHandlers(
    const std::vector<blink::Manifest::FileHandler>& manifest_file_handlers,
    WebApp& web_app) {
  apps::FileHandlers web_app_file_handlers;

  for (const auto& manifest_file_handler : manifest_file_handlers) {
    apps::FileHandler web_app_file_handler;
    web_app_file_handler.action = manifest_file_handler.action;

    for (const auto& it : manifest_file_handler.accept) {
      apps::FileHandler::AcceptEntry web_app_accept_entry;
      web_app_accept_entry.mime_type = base::UTF16ToUTF8(it.first);
      for (const auto& manifest_file_extension : it.second) {
        web_app_accept_entry.file_extensions.insert(
            base::UTF16ToUTF8(manifest_file_extension));
      }
      web_app_file_handler.accept.push_back(std::move(web_app_accept_entry));
    }

    web_app_file_handlers.push_back(std::move(web_app_file_handler));

    if (web_app_file_handlers.size() == kMaxFileHandlers &&
        !web_app.scope().SchemeIs(kChromeScheme)) {
      break;
    }
  }

  DCHECK(web_app_file_handlers.size() <= kMaxFileHandlers ||
         web_app.scope().SchemeIs(kChromeScheme));

  web_app.SetFileHandlers(std::move(web_app_file_handlers));
}

void SetWebAppProtocolHandlers(
    const std::vector<blink::Manifest::ProtocolHandler>& protocol_handlers,
    WebApp& web_app) {
  std::vector<apps::ProtocolHandlerInfo> web_app_protocol_handlers;
  for (const auto& handler : protocol_handlers) {
    apps::ProtocolHandlerInfo protocol_handler_info;
    protocol_handler_info.protocol = base::UTF16ToUTF8(handler.protocol);
    protocol_handler_info.url = handler.url;
    web_app_protocol_handlers.push_back(std::move(protocol_handler_info));
  }

  web_app.SetProtocolHandlers(web_app_protocol_handlers);
}

}  // namespace

void SetWebAppManifestFields(const WebApplicationInfo& web_app_info,
                             WebApp& web_app) {
  DCHECK(!web_app_info.title.empty());
  web_app.SetName(base::UTF16ToUTF8(web_app_info.title));

  web_app.SetDisplayMode(web_app_info.display_mode);
  web_app.SetDisplayModeOverride(web_app_info.display_override);

  web_app.SetDescription(base::UTF16ToUTF8(web_app_info.description));
  web_app.SetLaunchQueryParams(web_app_info.launch_query_params);
  web_app.SetScope(web_app_info.scope);
  DCHECK(!web_app_info.theme_color.has_value() ||
         SkColorGetA(*web_app_info.theme_color) == SK_AlphaOPAQUE);
  web_app.SetThemeColor(web_app_info.theme_color);
  DCHECK(!web_app_info.background_color.has_value() ||
         SkColorGetA(*web_app_info.background_color) == SK_AlphaOPAQUE);
  web_app.SetBackgroundColor(web_app_info.background_color);

  WebApp::SyncFallbackData sync_fallback_data;
  sync_fallback_data.name = base::UTF16ToUTF8(web_app_info.title);
  sync_fallback_data.theme_color = web_app_info.theme_color;
  sync_fallback_data.scope = web_app_info.scope;
  sync_fallback_data.icon_infos = web_app_info.icon_infos;
  web_app.SetSyncFallbackData(std::move(sync_fallback_data));

  web_app.SetIconInfos(web_app_info.icon_infos);
  web_app.SetDownloadedIconSizes(
      IconPurpose::ANY, GetSquareSizePxs(web_app_info.icon_bitmaps.any));
  web_app.SetDownloadedIconSizes(
      IconPurpose::MASKABLE,
      GetSquareSizePxs(web_app_info.icon_bitmaps.maskable));
  web_app.SetDownloadedIconSizes(
      IconPurpose::MONOCHROME,
      GetSquareSizePxs(web_app_info.icon_bitmaps.monochrome));
  web_app.SetIsGeneratedIcon(web_app_info.is_generated_icon);

  web_app.SetShortcutsMenuItemInfos(web_app_info.shortcuts_menu_item_infos);
  web_app.SetDownloadedShortcutsMenuIconsSizes(
      GetDownloadedShortcutsMenuIconsSizes(
          web_app_info.shortcuts_menu_icon_bitmaps));

  SetWebAppFileHandlers(web_app_info.file_handlers, web_app);
  web_app.SetShareTarget(web_app_info.share_target);
  SetWebAppProtocolHandlers(web_app_info.protocol_handlers, web_app);
  web_app.SetNoteTakingNewNoteUrl(web_app_info.note_taking_new_note_url);
  web_app.SetUrlHandlers(web_app_info.url_handlers);

  if (base::FeatureList::IsEnabled(features::kDesktopPWAsRunOnOsLogin) &&
      web_app_info.run_on_os_login) {
    // TODO(crbug.com/1091964): Obtain actual mode, currently set to the
    // default (windowed).
    web_app.SetRunOnOsLoginMode(RunOnOsLoginMode::kWindowed);
  }

  web_app.SetCaptureLinks(web_app_info.capture_links);

  web_app.SetManifestUrl(web_app_info.manifest_url);
}

void MaybeDisableOsIntegration(const AppRegistrar* app_registrar,
                               const AppId& app_id,
                               InstallOsHooksOptions* options) {
#if !defined(OS_CHROMEOS)  // Deeper OS integration is expected on ChromeOS.
  DCHECK(app_registrar);
  const WebAppRegistrar* web_app_registrar = app_registrar->AsWebAppRegistrar();

  // Disable OS integration if the app was installed by default only, and not
  // through any other means like an enterprise policy or store.
  if (web_app_registrar &&
      web_app_registrar->WasInstalledByDefaultOnly(app_id)) {
    options->add_to_desktop = false;
    options->add_to_quick_launch_bar = false;
    options->os_hooks[OsHookType::kShortcuts] = false;
    options->os_hooks[OsHookType::kRunOnOsLogin] = false;
    options->os_hooks[OsHookType::kShortcutsMenu] = false;
    options->os_hooks[OsHookType::kUninstallationViaOsSettings] = false;
    options->os_hooks[OsHookType::kFileHandlers] = false;
    options->os_hooks[OsHookType::kProtocolHandlers] = false;
    options->os_hooks[OsHookType::kUrlHandlers] = false;
  }
#endif  // !defined(OS_CHROMEOS)
}

}  // namespace web_app
