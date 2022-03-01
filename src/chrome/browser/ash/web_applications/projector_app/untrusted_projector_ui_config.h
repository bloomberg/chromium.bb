// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_PROJECTOR_APP_UNTRUSTED_PROJECTOR_UI_CONFIG_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_PROJECTOR_APP_UNTRUSTED_PROJECTOR_UI_CONFIG_H_

#include "ash/webui/projector_app/untrusted_projector_ui.h"
#include "ui/webui/webui_config.h"

namespace content {
class WebUIDataSource;
class WebUIController;
class WebUI;
}  // namespace content

// Implementation of the chromeos::UntrustedProjectorUIDelegate to expose some
// //chrome functions to //chromeos.
class ChromeUntrustedProjectorUIDelegate
    : public ash::UntrustedProjectorUIDelegate {
 public:
  ChromeUntrustedProjectorUIDelegate();
  ChromeUntrustedProjectorUIDelegate(
      const ChromeUntrustedProjectorUIDelegate&) = delete;
  ChromeUntrustedProjectorUIDelegate& operator=(
      const ChromeUntrustedProjectorUIDelegate&) = delete;

  // ash::UntrustedProjectorUIDelegate:
  void PopulateLoadTimeData(content::WebUIDataSource* source) override;
};

// A webui config for the chrome-untrusted:// part of Projector.
class UntrustedProjectorUIConfig : public ui::WebUIConfig {
 public:
  UntrustedProjectorUIConfig();
  UntrustedProjectorUIConfig(const UntrustedProjectorUIConfig& other) = delete;
  UntrustedProjectorUIConfig& operator=(const UntrustedProjectorUIConfig&) =
      delete;
  ~UntrustedProjectorUIConfig() override;

  // ui::WebUIConfig:
  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui) override;
};

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_PROJECTOR_APP_UNTRUSTED_PROJECTOR_UI_CONFIG_H_
