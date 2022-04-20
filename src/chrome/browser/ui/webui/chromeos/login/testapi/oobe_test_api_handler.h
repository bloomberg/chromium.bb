// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_TESTAPI_OOBE_TEST_API_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_TESTAPI_OOBE_TEST_API_HANDLER_H_

#include <string>
#include <vector>

#include "ash/public/mojom/cros_display_config.mojom.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/chromeos/login/base_webui_handler.h"

namespace chromeos {

class OobeTestAPIHandler : public BaseWebUIHandler {
 public:
  OobeTestAPIHandler();
  ~OobeTestAPIHandler() override;
  OobeTestAPIHandler(const OobeTestAPIHandler&) = delete;
  OobeTestAPIHandler& operator=(const OobeTestAPIHandler&) = delete;

  // WebUIMessageHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void DeclareJSCallbacks() override;
  void InitializeDeprecated() override;
  void GetAdditionalParameters(base::Value::Dict* dict) override;

 private:
  void LoginWithPin(const std::string& username, const std::string& pin);
  void AdvanceToScreen(const std::string& screen);
  void SkipPostLoginScreens();
  void LoginAsGuest();
  void ShowGaiaDialog();
  void HandleGetPrimaryDisplayName(const std::string& callback_id);
  void OnGetDisplayUnitInfoList(
      const std::string& callback_id,
      std::vector<ash::mojom::DisplayUnitInfoPtr> info_list);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_TESTAPI_OOBE_TEST_API_HANDLER_H_
