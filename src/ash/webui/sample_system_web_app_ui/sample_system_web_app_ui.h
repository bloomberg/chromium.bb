// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SAMPLE_SYSTEM_WEB_APP_UI_SAMPLE_SYSTEM_WEB_APP_UI_H_
#define ASH_WEBUI_SAMPLE_SYSTEM_WEB_APP_UI_SAMPLE_SYSTEM_WEB_APP_UI_H_

#if defined(OFFICIAL_BUILD)
#error Sample System Web App should only be included in unofficial builds.
#endif

#include <memory>

#include "ash/webui/sample_system_web_app_ui/mojom/sample_system_web_app_ui.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ash {

// The WebUI for chrome://sample-system-web-app/.
class SampleSystemWebAppUI : public ui::MojoWebUIController,
                             public mojom::sample_swa::PageHandlerFactory {
 public:
  explicit SampleSystemWebAppUI(content::WebUI* web_ui);
  SampleSystemWebAppUI(const SampleSystemWebAppUI&) = delete;
  SampleSystemWebAppUI& operator=(const SampleSystemWebAppUI&) = delete;
  ~SampleSystemWebAppUI() override;

  void BindInterface(
      mojo::PendingReceiver<mojom::sample_swa::PageHandlerFactory> factory);

 private:
  // mojom::sample_swa::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingReceiver<mojom::sample_swa::PageHandler> handler,
      mojo::PendingRemote<mojom::sample_swa::Page> page) override;

  mojo::Receiver<mojom::sample_swa::PageHandlerFactory> sample_page_factory_{
      this};

  // Handles requests from the user visible page. Created when the page calls
  // PageHandlerFactory::CreatePageHandler(). Expected to live as long as
  // the WebUIController. In most cases this matches the lifetime of the page.
  // However, sometimes the WebUIController is re-used within same-origin
  // navigations. Calling CreatePageHandler() multiple times will replace the
  // existing sample_page_handler_.
  std::unique_ptr<mojom::sample_swa::PageHandler> sample_page_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // ASH_WEBUI_SAMPLE_SYSTEM_WEB_APP_UI_SAMPLE_SYSTEM_WEB_APP_UI_H_
