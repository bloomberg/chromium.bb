// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LAUNCHER_INTERNALS_LAUNCHER_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LAUNCHER_INTERNALS_LAUNCHER_INTERNALS_UI_H_

#include "chrome/browser/ui/webui/chromeos/launcher_internals/launcher_internals.mojom.h"
#include "chrome/browser/ui/webui/chromeos/launcher_internals/launcher_internals_handler.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace chromeos {

// The WebUI controller for chrome://launcher-internals.
class LauncherInternalsUI
    : public ui::MojoWebUIController,
      public launcher_internals::mojom::PageHandlerFactory {
 public:
  explicit LauncherInternalsUI(content::WebUI* web_ui);
  ~LauncherInternalsUI() override;

  LauncherInternalsUI(const LauncherInternalsUI&) = delete;
  LauncherInternalsUI& operator=(const LauncherInternalsUI&) = delete;

  void BindInterface(
      mojo::PendingReceiver<launcher_internals::mojom::PageHandlerFactory>
          receiver);

 private:
  // launcher_internals::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<launcher_internals::mojom::Page> page) override;

  std::unique_ptr<LauncherInternalsHandler> page_handler_;
  mojo::Receiver<launcher_internals::mojom::PageHandlerFactory>
      factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LAUNCHER_INTERNALS_LAUNCHER_INTERNALS_UI_H_
