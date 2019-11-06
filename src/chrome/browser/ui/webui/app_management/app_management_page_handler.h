// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_APP_MANAGEMENT_APP_MANAGEMENT_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_APP_MANAGEMENT_APP_MANAGEMENT_PAGE_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/app_management/app_management.mojom.h"
#include "chrome/browser/ui/webui/app_management/app_management_shelf_delegate_chromeos.h"
#include "chrome/services/app_service/public/cpp/app_registry_cache.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace content {
class WebUI;
}

class Profile;

class AppManagementPageHandler : public app_management::mojom::PageHandler,
                                 public apps::AppRegistryCache::Observer {
 public:
  AppManagementPageHandler(app_management::mojom::PageHandlerRequest request,
                           app_management::mojom::PagePtr page,
                           content::WebUI* web_ui);
  ~AppManagementPageHandler() override;

  void OnPinnedChanged(const std::string& app_id, bool pinned);
  void OnArcSupportChanged(bool supported);

  // app_management::mojom::PageHandler:
  void GetApps(GetAppsCallback callback) override;
  void GetExtensionAppPermissionMessages(
      const std::string& app_id,
      GetExtensionAppPermissionMessagesCallback callback) override;
  void SetPinned(const std::string& app_id,
                 apps::mojom::OptionalBool pinned) override;
  void SetPermission(const std::string& app_id,
                     apps::mojom::PermissionPtr permission) override;
  void Uninstall(const std::string& app_id) override;
  void OpenNativeSettings(const std::string& app_id) override;

 private:
  app_management::mojom::AppPtr CreateUIAppPtr(const apps::AppUpdate& update);

  // apps::AppRegistryCache::Observer overrides:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  mojo::Binding<app_management::mojom::PageHandler> binding_;

  app_management::mojom::PagePtr page_;

  Profile* profile_;

#if defined(OS_CHROMEOS)
  AppManagementShelfDelegate shelf_delegate_;
#endif

  DISALLOW_COPY_AND_ASSIGN(AppManagementPageHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_APP_MANAGEMENT_APP_MANAGEMENT_PAGE_HANDLER_H_
