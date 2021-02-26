// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_CUSTOMIZATION_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_CUSTOMIZATION_UI_H_

#include "base/callback.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/customize_themes/customize_themes.mojom.h"

namespace content {
class WebUI;
}

class ChromeCustomizeThemesHandler;

// This WebUI uses mojo for the color picker element.
class ProfileCustomizationUI
    : public ui::MojoWebUIController,
      public customize_themes::mojom::CustomizeThemesHandlerFactory {
 public:
  explicit ProfileCustomizationUI(content::WebUI* web_ui);
  ~ProfileCustomizationUI() override;

  ProfileCustomizationUI(const ProfileCustomizationUI&) = delete;
  ProfileCustomizationUI& operator=(const ProfileCustomizationUI&) = delete;

  // Initializes the ProfileCustomizationUI.
  void Initialize(base::OnceClosure done_closure);

  // Instantiates the implementor of the
  // customize_themes::mojom::CustomizeThemesHandlerFactory mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(mojo::PendingReceiver<
                     customize_themes::mojom::CustomizeThemesHandlerFactory>
                         pending_receiver);

 private:
  // customize_themes::mojom::CustomizeThemesHandlerFactory:
  void CreateCustomizeThemesHandler(
      mojo::PendingRemote<customize_themes::mojom::CustomizeThemesClient>
          pending_client,
      mojo::PendingReceiver<customize_themes::mojom::CustomizeThemesHandler>
          pending_handler) override;

  std::unique_ptr<ChromeCustomizeThemesHandler> customize_themes_handler_;
  mojo::Receiver<customize_themes::mojom::CustomizeThemesHandlerFactory>
      customize_themes_factory_receiver_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_CUSTOMIZATION_UI_H_
