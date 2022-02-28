// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_HELP_APP_HELP_APP_UNTRUSTED_UI_CONFIG_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_HELP_APP_HELP_APP_UNTRUSTED_UI_CONFIG_H_

#include "ui/webui/webui_config.h"

namespace ash {

class HelpAppUntrustedUIConfig : public ui::WebUIConfig {
 public:
  HelpAppUntrustedUIConfig();
  HelpAppUntrustedUIConfig(const HelpAppUntrustedUIConfig& other) = delete;
  HelpAppUntrustedUIConfig& operator=(const HelpAppUntrustedUIConfig&) = delete;
  ~HelpAppUntrustedUIConfig() override;

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_HELP_APP_HELP_APP_UNTRUSTED_UI_CONFIG_H_
