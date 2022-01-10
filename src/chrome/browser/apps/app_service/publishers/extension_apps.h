// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_EXTENSION_APPS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_EXTENSION_APPS_H_

#include <string>

#include "chrome/browser/apps/app_service/app_icon/icon_key_util.h"
#include "chrome/browser/apps/app_service/publishers/extension_apps_base.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/mojom/app_service.mojom.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/gfx/native_widget_types.h"

namespace extensions {
class Extension;
}

class Profile;

namespace apps {

class PublisherHost;

// An app publisher (in the App Service sense) of extension-backed apps for
// Chrome, including Chrome Apps (platform apps and legacy packaged apps) and
// hosted apps.
//
// See components/services/app_service/README.md.
class ExtensionApps : public apps::ExtensionAppsBase {
 public:
  ExtensionApps(AppServiceProxy* proxy, AppType app_type);
  ~ExtensionApps() override;

  ExtensionApps(const ExtensionApps&) = delete;
  ExtensionApps& operator=(const ExtensionApps&) = delete;

  // Uninstall for web apps on Chrome.
  static void UninstallImpl(Profile* profile,
                            const std::string& app_id,
                            gfx::NativeWindow parent_window);

 private:
  friend class PublisherHost;

  // ExtensionAppsBase overrides.
  bool Accepts(const extensions::Extension* extension) override;
  bool ShouldShownInLauncher(const extensions::Extension* extension) override;
  std::unique_ptr<App> CreateApp(const extensions::Extension* extension,
                                 Readiness readiness) override;
  apps::mojom::AppPtr Convert(const extensions::Extension* extension,
                              apps::mojom::Readiness readiness) override;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_EXTENSION_APPS_H_
