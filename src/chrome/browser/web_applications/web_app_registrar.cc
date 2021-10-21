// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_registrar.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/web_applications/app_registrar_observer.h"
#include "chrome/browser/web_applications/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/install_bounce_metric.h"
#include "chrome/browser/web_applications/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_prefs_utils.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "content/public/common/content_features.h"

namespace web_app {

namespace {

// With Lacros, only system web apps are exposed using the Ash browser.
bool WebAppExposed(const WebApp& web_app) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (base::FeatureList::IsEnabled(features::kWebAppsCrosapi) &&
      !web_app.IsSystemApp()) {
    return false;
  }
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  if (web_app.IsSystemApp() && !AreSystemWebAppsSupported())
    return false;
#endif
  return true;
}

}  // namespace

WebAppRegistrar::WebAppRegistrar(Profile* profile) : profile_(profile) {}

WebAppRegistrar::~WebAppRegistrar() {
  for (AppRegistrarObserver& observer : observers_)
    observer.OnAppRegistrarDestroyed();
}

void WebAppRegistrar::SetSubsystems(
    OsIntegrationManager* os_integration_manager) {
  os_integration_manager_ = os_integration_manager;
}

bool WebAppRegistrar::IsLocallyInstalled(const GURL& start_url) const {
  return IsLocallyInstalled(
      GenerateAppId(/*manifest_id=*/absl::nullopt, start_url));
}

bool WebAppRegistrar::IsPlaceholderApp(const AppId& app_id) const {
  return ExternallyInstalledWebAppPrefs(profile_->GetPrefs())
      .IsPlaceholderApp(app_id);
}

void WebAppRegistrar::AddObserver(AppRegistrarObserver* observer) {
  observers_.AddObserver(observer);
}

void WebAppRegistrar::RemoveObserver(AppRegistrarObserver* observer) {
  observers_.RemoveObserver(observer);
}

void WebAppRegistrar::NotifyWebAppProtocolSettingsChanged() {
  for (AppRegistrarObserver& observer : observers_)
    observer.OnWebAppProtocolSettingsChanged();
}

void WebAppRegistrar::NotifyWebAppInstalled(const AppId& app_id) {
  for (AppRegistrarObserver& observer : observers_)
    observer.OnWebAppInstalled(app_id);
  // TODO(alancutter): Call RecordWebAppInstallation here when we get access to
  // the webapps::WebappInstallSource in this event.
}

void WebAppRegistrar::NotifyWebAppManifestUpdated(const AppId& app_id,
                                                  base::StringPiece old_name) {
  for (AppRegistrarObserver& observer : observers_)
    observer.OnWebAppManifestUpdated(app_id, old_name);
}

void WebAppRegistrar::NotifyWebAppsWillBeUpdatedFromSync(
    const std::vector<const WebApp*>& new_apps_state) {
  for (AppRegistrarObserver& observer : observers_)
    observer.OnWebAppsWillBeUpdatedFromSync(new_apps_state);
}

void WebAppRegistrar::NotifyWebAppUninstalled(const AppId& app_id) {
  for (AppRegistrarObserver& observer : observers_)
    observer.OnWebAppUninstalled(app_id);
}

void WebAppRegistrar::NotifyWebAppWillBeUninstalled(const AppId& app_id) {
  for (AppRegistrarObserver& observer : observers_)
    observer.OnWebAppWillBeUninstalled(app_id);
  RecordWebAppUninstallation(profile()->GetPrefs(), app_id);
}

void WebAppRegistrar::NotifyWebAppLocallyInstalledStateChanged(
    const AppId& app_id,
    bool is_locally_installed) {
  for (AppRegistrarObserver& observer : observers_)
    observer.OnWebAppLocallyInstalledStateChanged(app_id, is_locally_installed);
}

void WebAppRegistrar::NotifyWebAppDisabledStateChanged(const AppId& app_id,
                                                       bool is_disabled) {
  for (AppRegistrarObserver& observer : observers_)
    observer.OnWebAppDisabledStateChanged(app_id, is_disabled);
}

void WebAppRegistrar::NotifyWebAppsDisabledModeChanged() {
  for (AppRegistrarObserver& observer : observers_)
    observer.OnWebAppsDisabledModeChanged();
}

void WebAppRegistrar::NotifyWebAppLastBadgingTimeChanged(
    const AppId& app_id,
    const base::Time& time) {
  for (AppRegistrarObserver& observer : observers_)
    observer.OnWebAppLastBadgingTimeChanged(app_id, time);
}

void WebAppRegistrar::NotifyWebAppLastLaunchTimeChanged(
    const AppId& app_id,
    const base::Time& time) {
  for (AppRegistrarObserver& observer : observers_)
    observer.OnWebAppLastLaunchTimeChanged(app_id, time);
}

void WebAppRegistrar::NotifyWebAppInstallTimeChanged(const AppId& app_id,
                                                     const base::Time& time) {
  for (AppRegistrarObserver& observer : observers_)
    observer.OnWebAppInstallTimeChanged(app_id, time);
}

void WebAppRegistrar::NotifyWebAppProfileWillBeDeleted(const AppId& app_id) {
  for (AppRegistrarObserver& observer : observers_)
    observer.OnWebAppProfileWillBeDeleted(app_id);
}

void WebAppRegistrar::NotifyWebAppInstalledWithOsHooks(const AppId& app_id) {
  for (AppRegistrarObserver& observer : observers_)
    observer.OnWebAppInstalledWithOsHooks(app_id);
}

void WebAppRegistrar::NotifyWebAppUserDisplayModeChanged(
    const AppId& app_id,
    DisplayMode user_display_mode) {
  for (AppRegistrarObserver& observer : observers_)
    observer.OnWebAppUserDisplayModeChanged(app_id, user_display_mode);
}

void WebAppRegistrar::NotifyAppRegistrarShutdown() {
  for (AppRegistrarObserver& observer : observers_)
    observer.OnAppRegistrarShutdown();
}

std::map<AppId, GURL> WebAppRegistrar::GetExternallyInstalledApps(
    ExternalInstallSource install_source) const {
  std::map<AppId, GURL> installed_apps =
      ExternallyInstalledWebAppPrefs::BuildAppIdsMap(profile()->GetPrefs(),
                                                     install_source);
  base::EraseIf(installed_apps, [this](const std::pair<AppId, GURL>& app) {
    return !IsInstalled(app.first);
  });

  return installed_apps;
}

absl::optional<AppId> WebAppRegistrar::LookupExternalAppId(
    const GURL& install_url) const {
  return ExternallyInstalledWebAppPrefs(profile()->GetPrefs())
      .LookupAppId(install_url);
}

bool WebAppRegistrar::HasExternalApp(const AppId& app_id) const {
  return ExternallyInstalledWebAppPrefs::HasAppId(profile()->GetPrefs(),
                                                  app_id);
}

bool WebAppRegistrar::HasExternalAppWithInstallSource(
    const AppId& app_id,
    ExternalInstallSource install_source) const {
  return ExternallyInstalledWebAppPrefs::HasAppIdWithInstallSource(
      profile()->GetPrefs(), app_id, install_source);
}

GURL WebAppRegistrar::GetAppLaunchUrl(const AppId& app_id) const {
  const GURL& start_url = GetAppStartUrl(app_id);
  const std::string* launch_query_params = GetAppLaunchQueryParams(app_id);
  if (!start_url.is_valid() || !launch_query_params)
    return start_url;

  GURL::Replacements replacements;
  if (start_url.query_piece().empty()) {
    replacements.SetQueryStr(*launch_query_params);
    return start_url.ReplaceComponents(replacements);
  }

  if (start_url.query_piece().find(*launch_query_params) !=
      base::StringPiece::npos) {
    return start_url;
  }

  std::string query_params = start_url.query() + "&" + *launch_query_params;
  replacements.SetQueryStr(query_params);
  return start_url.ReplaceComponents(replacements);
}

GURL WebAppRegistrar::GetAppScope(const AppId& app_id) const {
  absl::optional<GURL> scope = GetAppScopeInternal(app_id);
  if (scope)
    return *scope;
  if (base::FeatureList::IsEnabled(
          features::kDesktopPWAsTabStripLinkCapturing) &&
      IsTabbedWindowModeEnabled(app_id)) {
    return GetAppStartUrl(app_id).GetOrigin();
  }
  return GetAppStartUrl(app_id).GetWithoutFilename();
}

absl::optional<AppId> WebAppRegistrar::FindAppWithUrlInScope(
    const GURL& url) const {
  const std::string url_path = url.spec();

  absl::optional<AppId> best_app_id;
  size_t best_app_path_length = 0U;
  bool best_app_is_shortcut = true;

  for (const AppId& app_id : GetAppIds()) {
    // TODO(crbug.com/910016): Treat shortcuts as PWAs.
    bool app_is_shortcut = IsShortcutApp(app_id);
    if (app_is_shortcut && !best_app_is_shortcut)
      continue;

    const std::string app_path = GetAppScope(app_id).spec();

    if ((app_path.size() > best_app_path_length ||
         (best_app_is_shortcut && !app_is_shortcut)) &&
        base::StartsWith(url_path, app_path, base::CompareCase::SENSITIVE)) {
      best_app_id = app_id;
      best_app_path_length = app_path.size();
      best_app_is_shortcut = app_is_shortcut;
    }
  }

  return best_app_id;
}

bool WebAppRegistrar::DoesScopeContainAnyApp(const GURL& scope) const {
  std::string scope_str = scope.spec();

  for (const auto& app_id : GetAppIds()) {
    if (!IsLocallyInstalled(app_id))
      continue;

    std::string app_scope = GetAppScope(app_id).spec();
    DCHECK(!app_scope.empty());

    if (base::StartsWith(app_scope, scope_str, base::CompareCase::SENSITIVE))
      return true;
  }

  return false;
}

std::vector<AppId> WebAppRegistrar::FindAppsInScope(const GURL& scope) const {
  std::string scope_str = scope.spec();

  std::vector<AppId> in_scope;
  for (const auto& app_id : GetAppIds()) {
    if (!IsLocallyInstalled(app_id))
      continue;

    std::string app_scope = GetAppScope(app_id).spec();
    DCHECK(!app_scope.empty());

    if (!base::StartsWith(app_scope, scope_str, base::CompareCase::SENSITIVE))
      continue;

    in_scope.push_back(app_id);
  }

  return in_scope;
}

absl::optional<AppId> WebAppRegistrar::FindInstalledAppWithUrlInScope(
    const GURL& url,
    bool window_only) const {
  const std::string url_path = url.spec();

  absl::optional<AppId> best_app_id;
  size_t best_app_path_length = 0U;
  bool best_app_is_shortcut = true;

  for (const AppId& app_id : GetAppIds()) {
    // TODO(crbug.com/910016): Treat shortcuts as PWAs.
    bool app_is_shortcut = IsShortcutApp(app_id);
    if (app_is_shortcut && !best_app_is_shortcut)
      continue;

    if (!IsLocallyInstalled(app_id))
      continue;

    if (window_only &&
        GetAppEffectiveDisplayMode(app_id) == DisplayMode::kBrowser) {
      continue;
    }

    const std::string app_path = GetAppScope(app_id).spec();

    if ((app_path.size() > best_app_path_length ||
         (best_app_is_shortcut && !app_is_shortcut)) &&
        base::StartsWith(url_path, app_path, base::CompareCase::SENSITIVE)) {
      best_app_id = app_id;
      best_app_path_length = app_path.size();
      best_app_is_shortcut = app_is_shortcut;
    }
  }

  return best_app_id;
}

bool WebAppRegistrar::IsShortcutApp(const AppId& app_id) const {
  // TODO (crbug/910016): Make app scope always return a value and record this
  //  distinction in some other way.
  return !GetAppScopeInternal(app_id).has_value();
}

DisplayMode WebAppRegistrar::GetAppEffectiveDisplayMode(
    const AppId& app_id) const {
  if (!IsLocallyInstalled(app_id))
    return DisplayMode::kBrowser;

  auto app_display_mode = GetAppDisplayMode(app_id);
  auto user_display_mode = GetAppUserDisplayMode(app_id);
  if (app_display_mode == DisplayMode::kUndefined ||
      user_display_mode == DisplayMode::kUndefined) {
    return DisplayMode::kUndefined;
  }

  std::vector<DisplayMode> display_mode_overrides =
      GetAppDisplayModeOverride(app_id);
  return ResolveEffectiveDisplayMode(app_display_mode, display_mode_overrides,
                                     user_display_mode);
}

DisplayMode WebAppRegistrar::GetEffectiveDisplayModeFromManifest(
    const AppId& app_id) const {
  std::vector<DisplayMode> display_mode_overrides =
      GetAppDisplayModeOverride(app_id);

  if (!display_mode_overrides.empty())
    return display_mode_overrides[0];

  return GetAppDisplayMode(app_id);
}

bool WebAppRegistrar::IsTabbedWindowModeEnabled(const AppId& app_id) const {
  if (!base::FeatureList::IsEnabled(features::kDesktopPWAsTabStrip))
    return false;
  return GetAppEffectiveDisplayMode(app_id) == DisplayMode::kTabbed;
}

const WebApp* WebAppRegistrar::GetAppById(const AppId& app_id) const {
  if (registry_profile_being_deleted_)
    return nullptr;

  auto it = registry_.find(app_id);
  if (it != registry_.end() && WebAppExposed(*it->second))
    return it->second.get();

  return nullptr;
}

const WebApp* WebAppRegistrar::GetAppByStartUrl(const GURL& start_url) const {
  if (registry_profile_being_deleted_)
    return nullptr;

  for (auto const& it : registry_) {
    if (WebAppExposed(*it.second) && it.second->start_url() == start_url)
      return it.second.get();
  }
  return nullptr;
}

std::vector<AppId> WebAppRegistrar::GetAppsFromSyncAndPendingInstallation() {
  AppSet apps_in_sync_install = AppSet(this, [](const WebApp& web_app) {
    return web_app.is_from_sync_and_pending_installation();
  });

  std::vector<AppId> app_ids;
  for (const WebApp& app : apps_in_sync_install)
    app_ids.push_back(app.app_id());

  return app_ids;
}

bool WebAppRegistrar::WasInstalledByDefaultOnly(const AppId& app_id) const {
  const WebApp* web_app = GetAppById(app_id);
  return web_app && web_app->HasOnlySource(Source::Type::kDefault);
}

void WebAppRegistrar::Start() {
  // Profile manager can be null in unit tests.
  if (g_browser_process->profile_manager())
    g_browser_process->profile_manager()->AddObserver(this);
}

void WebAppRegistrar::Shutdown() {
  if (g_browser_process->profile_manager())
    g_browser_process->profile_manager()->RemoveObserver(this);
}

bool WebAppRegistrar::IsInstalled(const AppId& app_id) const {
  const WebApp* web_app = GetAppById(app_id);
  return web_app && !web_app->is_from_sync_and_pending_installation();
}

bool WebAppRegistrar::IsUninstalling(const AppId& app_id) const {
  const WebApp* web_app = GetAppById(app_id);
  return web_app && web_app->is_uninstalling();
}

bool WebAppRegistrar::IsLocallyInstalled(const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->is_locally_installed() : false;
}

bool WebAppRegistrar::WasInstalledByUser(const AppId& app_id) const {
  const WebApp* web_app = GetAppById(app_id);
  return web_app && web_app->WasInstalledByUser();
}

bool WebAppRegistrar::WasInstalledByOem(const AppId& app_id) const {
  const WebApp* web_app = GetAppById(app_id);
  return web_app && web_app->chromeos_data().has_value() &&
         web_app->chromeos_data()->oem_installed;
}

bool WebAppRegistrar::IsAllowedLaunchProtocol(
    const AppId& app_id,
    std::string protocol_scheme) const {
  const WebApp* web_app = GetAppById(app_id);
  return web_app &&
         base::Contains(web_app->allowed_launch_protocols(), protocol_scheme);
}

bool WebAppRegistrar::IsDisallowedLaunchProtocol(
    const AppId& app_id,
    std::string protocol_scheme) const {
  const WebApp* web_app = GetAppById(app_id);
  return web_app && base::Contains(web_app->disallowed_launch_protocols(),
                                   protocol_scheme);
}

base::flat_set<std::string> WebAppRegistrar::GetAllAllowedLaunchProtocols()
    const {
  base::flat_set<std::string> protocols;
  for (const WebApp& web_app : GetApps()) {
    protocols.insert(web_app.allowed_launch_protocols().begin(),
                     web_app.allowed_launch_protocols().end());
  }
  return protocols;
}

base::flat_set<std::string> WebAppRegistrar::GetAllDisallowedLaunchProtocols()
    const {
  base::flat_set<std::string> protocols;
  for (const WebApp& web_app : GetApps()) {
    protocols.insert(web_app.disallowed_launch_protocols().begin(),
                     web_app.disallowed_launch_protocols().end());
  }
  return protocols;
}

int WebAppRegistrar::CountUserInstalledApps() const {
  int num_user_installed = 0;
  for (const WebApp& app : GetAppsIncludingStubs()) {
    if (app.is_locally_installed() && app.WasInstalledByUser())
      ++num_user_installed;
  }
  return num_user_installed;
}

std::string WebAppRegistrar::GetAppShortName(const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->name() : std::string();
}

std::string WebAppRegistrar::GetAppDescription(const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->description() : std::string();
}

absl::optional<SkColor> WebAppRegistrar::GetAppThemeColor(
    const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->theme_color() : absl::nullopt;
}

absl::optional<SkColor> WebAppRegistrar::GetAppBackgroundColor(
    const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->background_color() : absl::nullopt;
}

absl::optional<SkColor> WebAppRegistrar::GetAppDarkModeThemeColor(
    const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->dark_mode_theme_color() : absl::nullopt;
}

const GURL& WebAppRegistrar::GetAppStartUrl(const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->start_url() : GURL::EmptyGURL();
}

absl::optional<std::string> WebAppRegistrar::GetAppManifestId(
    const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->manifest_id() : absl::nullopt;
}

const std::string* WebAppRegistrar::GetAppLaunchQueryParams(
    const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->launch_query_params() : nullptr;
}

const apps::ShareTarget* WebAppRegistrar::GetAppShareTarget(
    const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return (web_app && web_app->share_target().has_value())
             ? &web_app->share_target().value()
             : nullptr;
}

blink::mojom::CaptureLinks WebAppRegistrar::GetAppCaptureLinks(
    const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->capture_links()
                 : blink::mojom::CaptureLinks::kUndefined;
}

const apps::FileHandlers* WebAppRegistrar::GetAppFileHandlers(
    const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? &web_app->file_handlers() : nullptr;
}

const apps::ProtocolHandlers* WebAppRegistrar::GetAppProtocolHandlers(
    const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? &web_app->protocol_handlers() : nullptr;
}

bool WebAppRegistrar::IsAppFileHandlerPermissionBlocked(
    const web_app::AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->file_handler_permission_blocked() : false;
}

absl::optional<GURL> WebAppRegistrar::GetAppScopeInternal(
    const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  if (!web_app)
    return absl::nullopt;

  // TODO(crbug.com/910016): Treat shortcuts as PWAs.
  // Shortcuts on the WebApp system have empty scopes, while the implementation
  // of IsShortcutApp just checks if the scope is |absl::nullopt|, so make sure
  // we return |absl::nullopt| rather than an empty scope.
  if (web_app->scope().is_empty())
    return absl::nullopt;

  return web_app->scope();
}

DisplayMode WebAppRegistrar::GetAppDisplayMode(const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->display_mode() : DisplayMode::kUndefined;
}

DisplayMode WebAppRegistrar::GetAppUserDisplayMode(const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->user_display_mode() : DisplayMode::kUndefined;
}

std::vector<DisplayMode> WebAppRegistrar::GetAppDisplayModeOverride(
    const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->display_mode_override()
                 : std::vector<DisplayMode>();
}

apps::UrlHandlers WebAppRegistrar::GetAppUrlHandlers(
    const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->url_handlers()
                 : std::vector<apps::UrlHandlerInfo>();
}

GURL WebAppRegistrar::GetAppManifestUrl(const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->manifest_url() : GURL::EmptyGURL();
}

base::Time WebAppRegistrar::GetAppLastBadgingTime(const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->last_badging_time() : base::Time();
}

base::Time WebAppRegistrar::GetAppLastLaunchTime(const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->last_launch_time() : base::Time();
}

base::Time WebAppRegistrar::GetAppInstallTime(const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->install_time() : base::Time();
}

std::vector<apps::IconInfo> WebAppRegistrar::GetAppIconInfos(
    const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->manifest_icons() : std::vector<apps::IconInfo>();
}

SortedSizesPx WebAppRegistrar::GetAppDownloadedIconSizesAny(
    const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->downloaded_icon_sizes(IconPurpose::ANY)
                 : SortedSizesPx();
}

std::vector<WebApplicationShortcutsMenuItemInfo>
WebAppRegistrar::GetAppShortcutsMenuItemInfos(const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->shortcuts_menu_item_infos()
                 : std::vector<WebApplicationShortcutsMenuItemInfo>();
}

std::vector<IconSizes> WebAppRegistrar::GetAppDownloadedShortcutsMenuIconsSizes(
    const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->downloaded_shortcuts_menu_icons_sizes()
                 : std::vector<IconSizes>();
}

std::vector<AppId> WebAppRegistrar::GetAppIds() const {
  std::vector<AppId> app_ids;

  for (const WebApp& app : GetApps())
    app_ids.push_back(app.app_id());

  return app_ids;
}

RunOnOsLoginMode WebAppRegistrar::GetAppRunOnOsLoginMode(
    const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->run_on_os_login_mode() : RunOnOsLoginMode::kNotRun;
}

bool WebAppRegistrar::GetWindowControlsOverlayEnabled(
    const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->window_controls_overlay_enabled() : false;
}

void WebAppRegistrar::OnProfileMarkedForPermanentDeletion(
    Profile* profile_to_be_deleted) {
  if (profile() != profile_to_be_deleted)
    return;

  for (const auto& app : GetAppsIncludingStubs()) {
    NotifyWebAppProfileWillBeDeleted(app.app_id());
    os_integration_manager().UninstallAllOsHooks(app.app_id(),
                                                  base::DoNothing());
  }
  // We can't do registry_.clear() here because it makes in-memory registry
  // diverged from the sync server registry and from the on-disk registry
  // (WebAppDatabase/LevelDB and "Web Applications" profile directory).
  registry_profile_being_deleted_ = true;
}

WebAppRegistrar::AppSet::AppSet(const WebAppRegistrar* registrar, Filter filter)
    : registrar_(registrar),
      filter_(filter)
#if DCHECK_IS_ON()
      ,
      mutations_count_(registrar->mutations_count_)
#endif
{
}

WebAppRegistrar::AppSet::~AppSet() {
#if DCHECK_IS_ON()
  DCHECK_EQ(mutations_count_, registrar_->mutations_count_);
#endif
}

WebAppRegistrar::AppSet::iterator WebAppRegistrar::AppSet::begin() {
  return iterator(registrar_->registry_.begin(), registrar_->registry_.end(),
                  filter_);
}

WebAppRegistrar::AppSet::iterator WebAppRegistrar::AppSet::end() {
  return iterator(registrar_->registry_.end(), registrar_->registry_.end(),
                  filter_);
}

WebAppRegistrar::AppSet::const_iterator WebAppRegistrar::AppSet::begin() const {
  return const_iterator(registrar_->registry_.begin(),
                        registrar_->registry_.end(), filter_);
}

WebAppRegistrar::AppSet::const_iterator WebAppRegistrar::AppSet::end() const {
  return const_iterator(registrar_->registry_.end(),
                        registrar_->registry_.end(), filter_);
}

const WebAppRegistrar::AppSet WebAppRegistrar::GetAppsIncludingStubs() const {
  return AppSet(this, nullptr);
}

const WebAppRegistrar::AppSet WebAppRegistrar::GetApps() const {
  return AppSet(this, [](const WebApp& web_app) {
    return WebAppExposed(web_app) &&
           !web_app.is_from_sync_and_pending_installation();
  });
}

void WebAppRegistrar::SetRegistry(Registry&& registry) {
  registry_ = std::move(registry);
}

const WebAppRegistrar::AppSet WebAppRegistrar::FilterApps(Filter filter) const {
  return AppSet(this, filter);
}

void WebAppRegistrar::CountMutation() {
#if DCHECK_IS_ON()
  ++mutations_count_;
#endif
}

WebAppRegistrarMutable::WebAppRegistrarMutable(Profile* profile)
    : WebAppRegistrar(profile) {}

WebAppRegistrarMutable::~WebAppRegistrarMutable() = default;

void WebAppRegistrarMutable::InitRegistry(Registry&& registry) {
  DCHECK(is_empty());
  SetRegistry(std::move(registry));
}

WebApp* WebAppRegistrarMutable::GetAppByIdMutable(const AppId& app_id) {
  return const_cast<WebApp*>(GetAppById(app_id));
}

WebAppRegistrar::AppSet WebAppRegistrarMutable::FilterAppsMutable(
    Filter filter) {
  return AppSet(this, filter);
}

WebAppRegistrar::AppSet WebAppRegistrarMutable::GetAppsIncludingStubsMutable() {
  return AppSet(this, nullptr);
}

WebAppRegistrar::AppSet WebAppRegistrarMutable::GetAppsMutable() {
  return AppSet(this, [](const WebApp& web_app) {
    return WebAppExposed(web_app) &&
           !web_app.is_from_sync_and_pending_installation();
  });
}

bool IsRegistryEqual(const Registry& registry, const Registry& registry2) {
  if (registry.size() != registry2.size())
    return false;

  for (auto& kv : registry) {
    const WebApp* web_app = kv.second.get();
    const WebApp* web_app2 = registry2.at(web_app->app_id()).get();
    if (*web_app != *web_app2)
      return false;
  }

  return true;
}

}  // namespace web_app
