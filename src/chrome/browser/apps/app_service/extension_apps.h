// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_EXTENSION_APPS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_EXTENSION_APPS_H_

#include <string>

#include "chrome/browser/apps/app_service/extension_apps_base.h"
#include "chrome/browser/apps/app_service/icon_key_util.h"
#include "chrome/services/app_service/public/mojom/app_service.mojom.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/gfx/native_widget_types.h"

namespace extensions {
class Extension;
}

class Profile;

namespace apps {

// An app publisher (in the App Service sense) of extension-backed apps for
// Chrome, including Chrome Apps (platform apps and legacy packaged apps) and
// hosted apps (including desktop PWAs).
//
// In the future, desktop PWAs will be migrated to a new system.
//
// See chrome/services/app_service/README.md.
class ExtensionApps : public apps::ExtensionAppsBase {
 public:
  ExtensionApps(const mojo::Remote<apps::mojom::AppService>& app_service,
                Profile* profile,
                apps::mojom::AppType app_type);
  ~ExtensionApps() override;

  ExtensionApps(const ExtensionApps&) = delete;
  ExtensionApps& operator=(const ExtensionApps&) = delete;

  // Uninstall for web apps on Chrome.
  static void UninstallImpl(Profile* profile,
                            const std::string& app_id,
                            gfx::NativeWindow parent_window);

 private:
  // ExtensionAppsBase overrides.
  bool Accepts(const extensions::Extension* extension) override;
  bool ShouldShownInLauncher(const extensions::Extension* extension) override;
  apps::mojom::AppPtr Convert(const extensions::Extension* extension,
                              apps::mojom::Readiness readiness) override;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_EXTENSION_APPS_H_
