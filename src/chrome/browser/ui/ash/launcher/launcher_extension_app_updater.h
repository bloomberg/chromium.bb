// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_EXTENSION_APP_UPDATER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_EXTENSION_APP_UPDATER_H_

#include "base/macros.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/ash/launcher/launcher_app_updater.h"
#include "extensions/browser/extension_registry_observer.h"

// TODO(khmel): this is not Launcher class. Consider moving this to proper
// place.
class LauncherExtensionAppUpdater
    : public LauncherAppUpdater,
      public extensions::ExtensionRegistryObserver,
      public ArcAppListPrefs::Observer {
 public:
  LauncherExtensionAppUpdater(Delegate* delegate,
                              content::BrowserContext* browser_context,
                              bool extensions_only);
  ~LauncherExtensionAppUpdater() override;

  // ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override;
  void OnShutdown(extensions::ExtensionRegistry* registry) override;

  // ArcAppListPrefs::Observer
  void OnPackageInstalled(
      const arc::mojom::ArcPackageInfo& package_info) override;
  void OnPackageRemoved(const std::string& package_name,
                        bool uninstalled) override;
  void OnPackageListInitialRefreshed() override;

 private:
  void StartObservingExtensionRegistry();
  void StopObservingExtensionRegistry();

  void UpdateApp(const std::string& app_id);
  void UpdateEquivalentApp(const std::string& arc_package_name);

  bool ShouldHandleExtension(const extensions::Extension* extension) const;

  extensions::ExtensionRegistry* extension_registry_ = nullptr;

  // Handles life-cycle events for extensions only if true, otherwise handles
  // life-cycle events for both Chrome apps and extensions.
  const bool extensions_only_;

  DISALLOW_COPY_AND_ASSIGN(LauncherExtensionAppUpdater);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_EXTENSION_APP_UPDATER_H_
