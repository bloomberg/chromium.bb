// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_APP_MANAGEMENT_APP_MANAGEMENT_PAGE_HANDLER_FACTORY_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_APP_MANAGEMENT_APP_MANAGEMENT_PAGE_HANDLER_FACTORY_H_

#include <memory>

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/resources/cr_components/app_management/app_management.mojom.h"

class Profile;

class AppManagementPageHandler;

namespace ash {
namespace settings {

class AppManagementPageHandlerFactory
    : public app_management::mojom::PageHandlerFactory {
 public:
  explicit AppManagementPageHandlerFactory(Profile* profile);

  AppManagementPageHandlerFactory(const AppManagementPageHandlerFactory&) =
      delete;
  AppManagementPageHandlerFactory& operator=(
      const AppManagementPageHandlerFactory&) = delete;

  ~AppManagementPageHandlerFactory() override;

  void Bind(mojo::PendingReceiver<app_management::mojom::PageHandlerFactory>
                receiver);

 private:
  // app_management::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<app_management::mojom::Page> page,
      mojo::PendingReceiver<app_management::mojom::PageHandler> receiver)
      override;

  std::unique_ptr<AppManagementPageHandler> page_handler_;

  mojo::Receiver<app_management::mojom::PageHandlerFactory>
      page_factory_receiver_{this};

  Profile* profile_;
};

}  // namespace settings
}  // namespace ash

// TODO(https://crbug.com/1164001): remove when the migration is finished.
namespace chromeos {
namespace settings {
using ::ash::settings::AppManagementPageHandlerFactory;
}
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_APP_MANAGEMENT_APP_MANAGEMENT_PAGE_HANDLER_FACTORY_H_
