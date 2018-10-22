// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_FOUNDATION_APP_SERVICE_APP_SERVICE_H_
#define CHROME_BROWSER_APPS_FOUNDATION_APP_SERVICE_APP_SERVICE_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/apps/foundation/app_service/public/mojom/app_registry.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"

class PrefService;

namespace apps {

class AppRegistry;

// The App Service is an intermediary between M customers (e.g. app launching
// UI, or sync) and N providers (also known as app platforms, e.g. PWAs, ARC++,
// and Crostini). Its motivation is to abstract over platform-specific
// implementations and allow customers to work with generic actions (e.g.
// querying for app data) that are forwarded to the appropriate provider.
class AppService : public service_manager::Service {
 public:
  AppService();
  ~AppService() override;

  // service_manager::Service:
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& source,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

 private:
  void BindAppRegistryRequest(apps::mojom::AppRegistryRequest request);

  bool IsInitializationComplete() const { return app_registry_ != nullptr; }

  void OnPrefServiceConnected(std::unique_ptr<PrefService> pref_service);

  service_manager::BinderRegistry binder_registry_;

  std::unique_ptr<AppRegistry> app_registry_;

  // Requests which will be serviced once initialization of the App Registry is
  // complete.
  std::vector<apps::mojom::AppRegistryRequest> pending_app_registry_requests_;

  DISALLOW_COPY_AND_ASSIGN(AppService);
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_FOUNDATION_APP_SERVICE_APP_SERVICE_H_
