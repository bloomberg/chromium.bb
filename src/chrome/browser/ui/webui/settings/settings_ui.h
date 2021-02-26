// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_UI_H_

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_load_timer.h"
#include "content/public/browser/web_ui_controller.h"

#if !defined(OS_CHROMEOS)
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/customize_themes/customize_themes.mojom.h"
#endif  // !defined(OS_CHROMEOS)

namespace content {
class WebUIMessageHandler;
}  // namespace content

namespace user_prefs {
class PrefRegistrySyncable;
}

#if !defined(OS_CHROMEOS)
class ChromeCustomizeThemesHandler;
#endif  // !defined(OS_CHROMEOS)

namespace settings {

// The WebUI handler for chrome://settings.
class SettingsUI :
#if !defined(OS_CHROMEOS)
    // chrome://settings/manageProfile which only exists on !OS_CHROMEOS
    // requires mojo bindings.
    public ui::MojoWebUIController,
    public customize_themes::mojom::CustomizeThemesHandlerFactory
#else   // !defined(OS_CHROMEOS)
    public content::WebUIController
#endif  // defined(OS_CHROMEOS)
{
 public:
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  explicit SettingsUI(content::WebUI* web_ui);
  ~SettingsUI() override;

#if defined(OS_CHROMEOS)
  // Initializes the WebUI message handlers for CrOS-specific settings that are
  // still shown in the browser settings UI.
  void InitBrowserSettingsWebUIHandlers();
#else   // defined(OS_CHROMEOS)
  // Instantiates the implementor of the
  // customize_themes::mojom::CustomizeThemesHandlerFactory mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(mojo::PendingReceiver<
                     customize_themes::mojom::CustomizeThemesHandlerFactory>
                         pending_receiver);
#endif  // !defined(OS_CHROMEOS)

 private:
  void AddSettingsPageUIHandler(
      std::unique_ptr<content::WebUIMessageHandler> handler);

  // Makes a request to show a HaTS survey.
  void TryShowHatsSurveyWithTimeout();

#if !defined(OS_CHROMEOS)
  // customize_themes::mojom::CustomizeThemesHandlerFactory:
  void CreateCustomizeThemesHandler(
      mojo::PendingRemote<customize_themes::mojom::CustomizeThemesClient>
          pending_client,
      mojo::PendingReceiver<customize_themes::mojom::CustomizeThemesHandler>
          pending_handler) override;

  std::unique_ptr<ChromeCustomizeThemesHandler> customize_themes_handler_;
  mojo::Receiver<customize_themes::mojom::CustomizeThemesHandlerFactory>
      customize_themes_factory_receiver_;
#endif  // !defined(OS_CHROMEOS)

  WebuiLoadTimer webui_load_timer_;

  WEB_UI_CONTROLLER_TYPE_DECL();

  DISALLOW_COPY_AND_ASSIGN(SettingsUI);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_UI_H_
