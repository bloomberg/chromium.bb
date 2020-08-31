// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_REGISTRAR_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_REGISTRAR_H_

#include "base/callback_forward.h"
#include "base/scoped_observer.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"

class Profile;

namespace extensions {

class Extension;

class BookmarkAppRegistrar : public web_app::AppRegistrar,
                             public ExtensionRegistryObserver {
 public:
  explicit BookmarkAppRegistrar(Profile* profile);
  ~BookmarkAppRegistrar() override;

  // AppRegistrar:
  bool IsInstalled(const web_app::AppId& app_id) const override;
  bool IsLocallyInstalled(const web_app::AppId& app_id) const override;
  bool WasInstalledByUser(const web_app::AppId& app_id) const override;
  int CountUserInstalledApps() const override;
  std::string GetAppShortName(const web_app::AppId& app_id) const override;
  std::string GetAppDescription(const web_app::AppId& app_id) const override;
  base::Optional<SkColor> GetAppThemeColor(
      const web_app::AppId& app_id) const override;
  const GURL& GetAppLaunchURL(const web_app::AppId& app_id) const override;
  base::Optional<GURL> GetAppScopeInternal(
      const web_app::AppId& app_id) const override;
  web_app::DisplayMode GetAppDisplayMode(
      const web_app::AppId& app_id) const override;
  web_app::DisplayMode GetAppUserDisplayMode(
      const web_app::AppId& app_id) const override;
  std::vector<WebApplicationIconInfo> GetAppIconInfos(
      const web_app::AppId& app_id) const override;
  std::vector<SquareSizePx> GetAppDownloadedIconSizes(
      const web_app::AppId& app_id) const override;
  std::vector<web_app::AppId> GetAppIds() const override;
  web_app::WebAppRegistrar* AsWebAppRegistrar() override;
  BookmarkAppRegistrar* AsBookmarkAppRegistrar() override;

  // ExtensionRegistryObserver:
  // OnExtensionInstalled is not handled here.
  // AppRegistrar::NotifyWebAppInstalled is triggered by
  // BookmarkAppInstallFinalizer::OnExtensionInstalled().
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const Extension* extension,
                              UninstallReason reason) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;
  void OnShutdown(ExtensionRegistry* registry) override;

  // Finds the extension object in ExtensionRegistry and in the being
  // uninstalled slot.
  //
  // When AppRegistrarObserver::OnWebAppUninstalled(app_id) happens for
  // bookmark apps, the bookmark app backing that app_id is already removed
  // from ExtensionRegistry. If some abstract observer needs the extension
  // pointer for |app_id| being uninstalled, that observer should use this
  // getter. This is a short-term workaround which helps to unify
  // ExtensionsRegistry and WebAppRegistrar observation.
  const Extension* FindExtension(const web_app::AppId& app_id) const;

 private:
  // DCHECKs that app_id isn't for a Chrome app to catch places where Chrome app
  // UI accidentally starts using web_app::AppRegistrar when it shouldn't.
  const Extension* GetBookmarkAppDchecked(const web_app::AppId& app_id) const;
  const Extension* GetEnabledExtension(const web_app::AppId& app_id) const;

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_observer_{this};

  // Observers may find this pointer via FindExtension method.
  const Extension* bookmark_app_being_observed_ = nullptr;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_REGISTRAR_H_
