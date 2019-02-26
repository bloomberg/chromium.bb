// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_APP_SERVICE_APP_SERVICE_H_
#define CHROME_SERVICES_APP_SERVICE_APP_SERVICE_H_

#include <string>

#include "base/macros.h"
#include "chrome/services/app_service/app_service_impl.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"

namespace apps {

// The service (in the service_manager::Service sense) aspect of the App
// Service. For the implementation of the apps::mojom::AppService Mojo
// interface, see the AppServiceImpl class.
//
// See chrome/services/app_service/README.md.
class AppService : public service_manager::Service {
 public:
  AppService();
  ~AppService() override;

  // service_manager::Service overrides.
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& source,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

 private:
  service_manager::BinderRegistry binder_registry_;

  AppServiceImpl impl_;

  DISALLOW_COPY_AND_ASSIGN(AppService);
};

}  // namespace apps

#endif  // CHROME_SERVICES_APP_SERVICE_APP_SERVICE_H_
