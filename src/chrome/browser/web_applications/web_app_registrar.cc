// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_registrar.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check_op.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app.h"

namespace web_app {

WebAppRegistrar::WebAppRegistrar(Profile* profile) : AppRegistrar(profile) {}

WebAppRegistrar::~WebAppRegistrar() = default;

const WebApp* WebAppRegistrar::GetAppById(const AppId& app_id) const {
  if (registry_profile_being_deleted_)
    return nullptr;

  auto it = registry_.find(app_id);
  return it == registry_.end() ? nullptr : it->second.get();
}

const WebApp* WebAppRegistrar::GetAppByStartUrl(const GURL& start_url) const {
  if (registry_profile_being_deleted_)
    return nullptr;

  for (auto const& it : registry_) {
    if (it.second->start_url() == start_url)
      return it.second.get();
  }
  return nullptr;
}

std::vector<AppId> WebAppRegistrar::GetAppsInSyncInstall() {
  AppSet apps_in_sync_install = AppSet(
      this, [](const WebApp& web_app) { return web_app.is_in_sync_install(); });

  std::vector<AppId> app_ids;
  for (const WebApp& app : apps_in_sync_install)
    app_ids.push_back(app.app_id());

  return app_ids;
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
  return web_app && !web_app->is_in_sync_install();
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

base::Optional<SkColor> WebAppRegistrar::GetAppThemeColor(
    const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->theme_color() : base::nullopt;
}

base::Optional<SkColor> WebAppRegistrar::GetAppBackgroundColor(
    const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->background_color() : base::nullopt;
}

const GURL& WebAppRegistrar::GetAppStartUrl(const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->start_url() : GURL::EmptyGURL();
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

base::Optional<GURL> WebAppRegistrar::GetAppScopeInternal(
    const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  if (!web_app)
    return base::nullopt;

  // TODO(crbug.com/910016): Treat shortcuts as PWAs.
  // Shortcuts on the WebApp system have empty scopes, while the implementation
  // of IsShortcutApp just checks if the scope is |base::nullopt|, so make sure
  // we return |base::nullopt| rather than an empty scope.
  if (web_app->scope().is_empty())
    return base::nullopt;

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

std::vector<WebApplicationIconInfo> WebAppRegistrar::GetAppIconInfos(
    const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->icon_infos()
                 : std::vector<WebApplicationIconInfo>();
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

WebAppRegistrar* WebAppRegistrar::AsWebAppRegistrar() {
  return this;
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
    return !web_app.is_in_sync_install();
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
    return !web_app.is_in_sync_install();
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
