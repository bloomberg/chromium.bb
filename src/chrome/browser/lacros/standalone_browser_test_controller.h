// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_STANDALONE_BROWSER_TEST_CONTROLLER_H_
#define CHROME_BROWSER_LACROS_STANDALONE_BROWSER_TEST_CONTROLLER_H_

#include <string>

#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

// Created in lacros-chrome and registered with ash-chrome's test controller
// over crosapi to let the Ash browser tests that require Lacros to send
// commands to this lacros-chrome instance.
class StandaloneBrowserTestController
    : public crosapi::mojom::StandaloneBrowserTestController {
 public:
  explicit StandaloneBrowserTestController(
      mojo::Remote<crosapi::mojom::TestController>& test_controller);
  ~StandaloneBrowserTestController() override;

  void InstallWebApp(const std::string& start_url,
                     apps::mojom::WindowMode window_mode,
                     InstallWebAppCallback callback) override;

 private:
  void WebAppInstallationDone(InstallWebAppCallback callback,
                              const web_app::AppId& installed_app_id,
                              web_app::InstallResultCode code);

  mojo::Receiver<crosapi::mojom::StandaloneBrowserTestController>
      controller_receiver_{this};

  base::WeakPtrFactory<StandaloneBrowserTestController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_LACROS_STANDALONE_BROWSER_TEST_CONTROLLER_H_
