// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_EXTENSION_APPS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_EXTENSION_APPS_H_

#include <string>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/apps/app_service/icon_key_util.h"
#include "chrome/services/app_service/public/mojom/app_service.mojom.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "extensions/browser/extension_registry_observer.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"

class Profile;

namespace extensions {
class ExtensionSet;
}

namespace apps {

// An app publisher (in the App Service sense) of extension-backed apps,
// including Chrome Apps (platform apps and legacy packaged apps) and hosted
// apps (including desktop PWAs).
//
// In the future, desktop PWAs will be migrated to a new system.
//
// See chrome/services/app_service/README.md.
class ExtensionApps : public apps::mojom::Publisher,
                      public extensions::ExtensionRegistryObserver,
                      public content_settings::Observer {
 public:
  ExtensionApps();
  ~ExtensionApps() override;

  void Initialize(const apps::mojom::AppServicePtr& app_service,
                  Profile* profile,
                  apps::mojom::AppType type);
  void Shutdown();

 private:
  // Determines whether the given extension should be treated as type app_type_,
  // and should therefore by handled by this publisher.
  bool Accepts(const extensions::Extension* extension);

  // apps::mojom::Publisher overrides.
  void Connect(apps::mojom::SubscriberPtr subscriber,
               apps::mojom::ConnectOptionsPtr opts) override;
  void LoadIcon(apps::mojom::IconKeyPtr icon_key,
                apps::mojom::IconCompression icon_compression,
                int32_t size_hint_in_dip,
                bool allow_placeholder_icon,
                LoadIconCallback callback) override;
  void Launch(const std::string& app_id,
              int32_t event_flags,
              apps::mojom::LaunchSource launch_source,
              int64_t display_id) override;
  void SetPermission(const std::string& app_id,
                     apps::mojom::PermissionPtr permission) override;
  void Uninstall(const std::string& app_id) override;
  void OpenNativeSettings(const std::string& app_id) override;

  // content_settings::Observer overrides.
  void OnContentSettingChanged(const ContentSettingsPattern& primary_pattern,
                               const ContentSettingsPattern& secondary_pattern,
                               ContentSettingsType content_type,
                               const std::string& resource_identifier) override;

  // extensions::ExtensionRegistryObserver overrides.
  void OnExtensionInstalled(content::BrowserContext* browser_context,
                            const extensions::Extension* extension,
                            bool is_update) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override;

  void Publish(apps::mojom::AppPtr app);

  // Checks if extension is disabled and if enable flow should be started.
  // Returns true if extension enable flow is started or there is already one
  // running.
  bool RunExtensionEnableFlow(const std::string& app_id);

  static bool IsBlacklisted(const std::string& app_id);
  static apps::mojom::OptionalBool ShouldShowInAppManagement(
      const extensions::Extension* extension);
  void PopulatePermissions(const extensions::Extension* extension,
                           std::vector<mojom::PermissionPtr>* target);
  apps::mojom::AppPtr Convert(const extensions::Extension* extension,
                              apps::mojom::Readiness readiness);
  void ConvertVector(const extensions::ExtensionSet& extensions,
                     apps::mojom::Readiness readiness,
                     std::vector<apps::mojom::AppPtr>* apps_out);

  mojo::Binding<apps::mojom::Publisher> binding_;
  mojo::InterfacePtrSet<apps::mojom::Subscriber> subscribers_;

  Profile* profile_;

  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      observer_;

  apps_util::IncrementingIconKeyFactory icon_key_factory_;

  apps::mojom::AppType app_type_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionApps);
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_EXTENSION_APPS_H_
