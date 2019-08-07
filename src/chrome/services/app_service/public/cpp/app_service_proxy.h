// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_SERVICE_PROXY_H_
#define CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_SERVICE_PROXY_H_

#include <memory>

#include "base/macros.h"
#include "chrome/services/app_service/public/cpp/app_registry_cache.h"
#include "chrome/services/app_service/public/cpp/icon_loader.h"
#include "chrome/services/app_service/public/mojom/app_service.mojom.h"

namespace apps {

// Abstract superclass (with default no-op methods) for the App Service proxy.
// See chrome/services/app_service/README.md.
//
// Most code, outside of tests, should use an AppServiceProxyImpl.
class AppServiceProxy : public apps::IconLoader {
 public:
  AppServiceProxy();
  ~AppServiceProxy() override;

  apps::mojom::AppServicePtr& AppService();
  apps::AppRegistryCache& AppRegistryCache();

  // apps::IconLoader overrides.
  apps::mojom::IconKeyPtr GetIconKey(const std::string& app_id) override;
  std::unique_ptr<IconLoader::Releaser> LoadIconFromIconKey(
      apps::mojom::AppType app_type,
      const std::string& app_id,
      apps::mojom::IconKeyPtr icon_key,
      apps::mojom::IconCompression icon_compression,
      int32_t size_hint_in_dip,
      bool allow_placeholder_icon,
      apps::mojom::Publisher::LoadIconCallback callback) override;

  virtual void Launch(const std::string& app_id,
                      int32_t event_flags,
                      apps::mojom::LaunchSource launch_source,
                      int64_t display_id);

  virtual void SetPermission(const std::string& app_id,
                             apps::mojom::PermissionPtr permission);

  virtual void Uninstall(const std::string& app_id);

  virtual void OpenNativeSettings(const std::string& app_id);

 protected:
  apps::mojom::AppServicePtr app_service_;
  apps::AppRegistryCache cache_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppServiceProxy);
};

}  // namespace apps

#endif  // CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_SERVICE_PROXY_H_
