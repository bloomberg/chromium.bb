// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_APP_MANAGEMENT_APP_MANAGEMENT_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_APP_MANAGEMENT_APP_MANAGEMENT_PAGE_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/app_management/app_management.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace content {
class WebUI;
}

class AppManagementPageHandler : public app_management::mojom::PageHandler {
 public:
  AppManagementPageHandler(app_management::mojom::PageHandlerRequest request,
                           app_management::mojom::PagePtr page,
                           content::WebUI* web_ui);
  ~AppManagementPageHandler() override;

  // app_management::mojom::PageHandler:
  void GetApps() override;

 private:
  mojo::Binding<app_management::mojom::PageHandler> binding_;

  app_management::mojom::PagePtr page_;

  DISALLOW_COPY_AND_ASSIGN(AppManagementPageHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_APP_MANAGEMENT_APP_MANAGEMENT_PAGE_HANDLER_H_
