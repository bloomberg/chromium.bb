// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_KIOSK_ENABLE_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_KIOSK_ENABLE_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace ash {
class KioskEnableScreen;
}

namespace chromeos {

// Interface between enable kiosk screen and its representation.
// Note, do not forget to call OnViewDestroyed in the dtor.
class KioskEnableScreenView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"kiosk-enable"};

  virtual ~KioskEnableScreenView() {}

  virtual void Show() = 0;
  virtual void SetScreen(ash::KioskEnableScreen* screen) = 0;
  virtual void ShowKioskEnabled(bool success) = 0;
};

// WebUI implementation of KioskEnableScreenActor.
class KioskEnableScreenHandler : public KioskEnableScreenView,
                                 public BaseScreenHandler {
 public:
  using TView = KioskEnableScreenView;

  explicit KioskEnableScreenHandler(JSCallsContainer* js_calls_container);

  KioskEnableScreenHandler(const KioskEnableScreenHandler&) = delete;
  KioskEnableScreenHandler& operator=(const KioskEnableScreenHandler&) = delete;

  ~KioskEnableScreenHandler() override;

  // KioskEnableScreenView:
  void Show() override;
  void SetScreen(ash::KioskEnableScreen* screen) override;
  void ShowKioskEnabled(bool success) override;

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void Initialize() override;

 private:
  ash::KioskEnableScreen* screen_ = nullptr;

  // Keeps whether screen should be shown right after initialization.
  bool show_on_init_ = false;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::KioskEnableScreenHandler;
using ::chromeos::KioskEnableScreenView;
}

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_KIOSK_ENABLE_SCREEN_HANDLER_H_
