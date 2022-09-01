// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SMART_PRIVACY_PROTECTION_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SMART_PRIVACY_PROTECTION_SCREEN_HANDLER_H_

#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace ash {
class SmartPrivacyProtectionScreen;
}

namespace chromeos {

// Interface between SmartPrivacyProtection screen and its representation,
// either WebUI or Views one.
class SmartPrivacyProtectionView
    : public base::SupportsWeakPtr<SmartPrivacyProtectionView> {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{
      "smart-privacy-protection", "SmartPrivacyProtectionScreen"};

  virtual ~SmartPrivacyProtectionView() {}

  virtual void Show() = 0;
};

// WebUI implementation of SmartPrivacyProtectionView. It is used to interact
// with the SmartPrivacyProtection part of the JS page.
class SmartPrivacyProtectionScreenHandler : public SmartPrivacyProtectionView,
                                            public BaseScreenHandler {
 public:
  using TView = SmartPrivacyProtectionView;

  SmartPrivacyProtectionScreenHandler();

  SmartPrivacyProtectionScreenHandler(
      const SmartPrivacyProtectionScreenHandler&) = delete;
  SmartPrivacyProtectionScreenHandler& operator=(
      const SmartPrivacyProtectionScreenHandler&) = delete;

  ~SmartPrivacyProtectionScreenHandler() override;

  // SmartPrivacyProtectionView implementation:
  void Show() override;

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void GetAdditionalParameters(base::Value::Dict* dict) override;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::SmartPrivacyProtectionScreenHandler;
using ::chromeos::SmartPrivacyProtectionView;
}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SMART_PRIVACY_PROTECTION_SCREEN_HANDLER_H_
