// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_registrar.h"

#include <utility>

#include "base/one_shot_event.h"
#include "chrome/browser/extensions/convert_web_app.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/app_registrar_observer.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_util.h"
#include "chrome/common/extensions/api/url_handlers/url_handlers_parser.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/extensions/manifest_handlers/app_theme_color_info.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "url/gurl.h"

namespace extensions {

BookmarkAppRegistrar::BookmarkAppRegistrar(Profile* profile)
    : AppRegistrar(profile) {
  extension_observer_.Add(ExtensionRegistry::Get(profile));
}

BookmarkAppRegistrar::~BookmarkAppRegistrar() = default;

void BookmarkAppRegistrar::Init(base::OnceClosure callback) {
  ExtensionSystem::Get(profile())->ready().Post(FROM_HERE, std::move(callback));
}

BookmarkAppRegistrar* BookmarkAppRegistrar::AsBookmarkAppRegistrar() {
  return this;
}

bool BookmarkAppRegistrar::IsInstalled(const web_app::AppId& app_id) const {
  return GetExtension(app_id) != nullptr;
}

bool BookmarkAppRegistrar::IsLocallyInstalled(
    const web_app::AppId& app_id) const {
  const Extension* extension = GetExtension(app_id);
  return extension && BookmarkAppIsLocallyInstalled(profile(), extension);
}

bool BookmarkAppRegistrar::WasExternalAppUninstalledByUser(
    const web_app::AppId& app_id) const {
  return ExtensionPrefs::Get(profile())->IsExternalExtensionUninstalled(app_id);
}

bool BookmarkAppRegistrar::WasInstalledByUser(
    const web_app::AppId& app_id) const {
  const Extension* extension = GetExtension(app_id);
  return extension && !extension->was_installed_by_default();
}

base::Optional<web_app::AppId> BookmarkAppRegistrar::FindAppWithUrlInScope(
    const GURL& url) const {
  const Extension* extension = util::GetInstalledPwaForUrl(profile(), url);

  if (!extension)
    extension = GetInstalledShortcutForUrl(profile(), url);

  if (extension)
    return extension->id();

  return base::nullopt;
}

int BookmarkAppRegistrar::CountUserInstalledApps() const {
  return CountUserInstalledBookmarkApps(profile());
}

void BookmarkAppRegistrar::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  DCHECK_EQ(browser_context, profile());
  if (extension->from_bookmark())
    NotifyWebAppUninstalled(extension->id());
}

void BookmarkAppRegistrar::OnShutdown(ExtensionRegistry* registry) {
  NotifyAppRegistrarShutdown();
  extension_observer_.RemoveAll();
}

const Extension* BookmarkAppRegistrar::GetExtension(
    const web_app::AppId& app_id) const {
  const Extension* extension =
      ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(app_id);
  DCHECK(!extension || extension->from_bookmark());
  return extension;
}

std::string BookmarkAppRegistrar::GetAppShortName(
    const web_app::AppId& app_id) const {
  const Extension* extension = GetExtension(app_id);
  return extension ? extension->short_name() : std::string();
}

std::string BookmarkAppRegistrar::GetAppDescription(
    const web_app::AppId& app_id) const {
  const Extension* extension = GetExtension(app_id);
  return extension ? extension->description() : std::string();
}

base::Optional<SkColor> BookmarkAppRegistrar::GetAppThemeColor(
    const web_app::AppId& app_id) const {
  const Extension* extension = GetExtension(app_id);
  if (!extension)
    return base::nullopt;

  base::Optional<SkColor> extension_theme_color =
      AppThemeColorInfo::GetThemeColor(extension);
  if (extension_theme_color)
    return SkColorSetA(*extension_theme_color, SK_AlphaOPAQUE);

  return base::nullopt;
}

const GURL& BookmarkAppRegistrar::GetAppLaunchURL(
    const web_app::AppId& app_id) const {
  const Extension* extension = GetExtension(app_id);
  return extension ? AppLaunchInfo::GetLaunchWebURL(extension)
                   : GURL::EmptyGURL();
}

base::Optional<GURL> BookmarkAppRegistrar::GetAppScope(
    const web_app::AppId& app_id) const {
  const Extension* extension = GetExtension(app_id);
  if (!extension)
    return base::nullopt;

  GURL scope_url = GetScopeURLFromBookmarkApp(GetExtension(app_id));
  if (scope_url.is_valid())
    return scope_url;

  return base::nullopt;
}

web_app::LaunchContainer BookmarkAppRegistrar::GetAppLaunchContainer(
    const web_app::AppId& app_id) const {
  const Extension* extension = GetExtension(app_id);
  if (!extension)
    return web_app::LaunchContainer::kWindow;

  switch (extensions::GetLaunchContainer(
      extensions::ExtensionPrefs::Get(profile()), extension)) {
    case LaunchContainer::kLaunchContainerWindow:
    case LaunchContainer::kLaunchContainerPanelDeprecated:
      return web_app::LaunchContainer::kWindow;
    case LaunchContainer::kLaunchContainerTab:
      return web_app::LaunchContainer::kTab;
    case LaunchContainer::kLaunchContainerNone:
      NOTREACHED();
      return web_app::LaunchContainer::kDefault;
  }
}

void BookmarkAppRegistrar::SetAppLaunchContainer(
    const web_app::AppId& app_id,
    web_app::LaunchContainer launch_container) {
  const Extension* extension = GetExtension(app_id);
  DCHECK(extension);
  if (!extension)
    return;

  switch (launch_container) {
    case web_app::LaunchContainer::kWindow:
      extensions::SetLaunchType(profile(), extension->id(),
                                extensions::LAUNCH_TYPE_WINDOW);
      return;
    case web_app::LaunchContainer::kTab:
      extensions::SetLaunchType(profile(), extension->id(),
                                extensions::LAUNCH_TYPE_REGULAR);
      return;
    case web_app::LaunchContainer::kDefault:
      NOTREACHED();
      return;
  }
}

std::vector<web_app::AppId> BookmarkAppRegistrar::GetAppIds() const {
  std::vector<web_app::AppId> app_ids;
  for (scoped_refptr<const Extension> app :
       ExtensionRegistry::Get(profile())->enabled_extensions()) {
    if (app->from_bookmark()) {
      app_ids.push_back(app->id());
    }
  }
  return app_ids;
}

}  // namespace extensions
